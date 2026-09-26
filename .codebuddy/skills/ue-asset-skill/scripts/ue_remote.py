# -*- coding: utf-8 -*-
"""UE Remote Execution 通信模块。

通过 UE4/UE5 内置的 Python Remote Execution 协议与本机运行中的 UE 编辑器通信。
仅使用 Python 标准库，无需第三方依赖。

本模块为 ue-asset-skill 脚本提供底层通信能力：
  - 首次调用完整执行"扫描 → 反连 → 发送 → 保持连接"；
  - 连接意外断开时自动重建一次（对调用方透明）。

前置条件:
  1. UE 编辑器已启动
  2. 已启用 Python Remote Execution:
     Edit -> Project Settings -> Plugins -> Python
     -> Remote Execution -> Enable Remote Execution

公开 API:
    send_command(python_code, timeout=6.0, read_timeout=120.0) -> dict
    close_session() -> None
    is_connected() -> bool
    get_connection_info() -> dict
"""

import atexit
import json
import socket
import struct
import time
import uuid
from typing import Any, Dict, Optional, Tuple

# ====================================================================
# 协议常量
# ====================================================================
_PROTOCOL_VERSION = 1
_PROTOCOL_MAGIC = "ue_py"
_MULTICAST_GROUP = ("239.0.0.1", 6766)
_MULTICAST_TTL = 0  # 仅本机
_COMMAND_IP = "127.0.0.1"
_COMMAND_PORT_START = 6776
_DEFAULT_READ_TIMEOUT = 120.0


# ====================================================================
# 内部通信实现
# ====================================================================


def _find_free_port(start: int = _COMMAND_PORT_START, count: int = 1000) -> Optional[int]:
    """在指定范围内找到一个可用的 TCP 端口。"""
    for p in range(start, start + count):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((_COMMAND_IP, p))
            s.close()
            return p
        except OSError:
            continue
    return None


def _create_multicast_listener() -> socket.socket:
    """创建 UDP 组播监听 socket，用于接收 pong 响应。"""
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    udp.bind(("", _MULTICAST_GROUP[1]))
    mreq = struct.pack(
        "4s4s",
        socket.inet_aton(_MULTICAST_GROUP[0]),
        socket.inet_aton("0.0.0.0"),
    )
    udp.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    udp.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, _MULTICAST_TTL)
    return udp


def _create_udp_sender(ttl: int = 0) -> socket.socket:
    """创建 UDP 组播发送 socket。"""
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    udp.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
    return udp


def _get_local_ips() -> list:
    """获取本机所有 IPv4 地址，用于多网卡组播广播。"""
    ips = set(["127.0.0.1"])
    try:
        hostname = socket.gethostname()
        _, _, host_ips = socket.gethostbyname_ex(hostname)
        ips.update(host_ips)
    except Exception:
        pass
    return list(ips)


def _send_multicast(udp_sock: socket.socket, msg: bytes) -> None:
    """向所有本地网卡发送组播包，解决多网卡/虚拟网卡路由冲突问题。"""
    for ip in _get_local_ips():
        try:
            udp_sock.setsockopt(
                socket.IPPROTO_IP,
                socket.IP_MULTICAST_IF,
                socket.inet_aton(ip)
            )
            udp_sock.sendto(msg, _MULTICAST_GROUP)
        except Exception:
            pass


def _scan_local_node(timeout: float = 6.0) -> Optional[Dict[str, Any]]:
    """通过 UDP 组播扫描并返回本机 UE 编辑器节点信息。"""
    my_id = str(uuid.uuid4())
    local_hostname = socket.gethostname().upper()

    udp = _create_multicast_listener()
    udp.settimeout(1.0)

    ping_msg = json.dumps({
        "version": _PROTOCOL_VERSION,
        "magic": _PROTOCOL_MAGIC,
        "type": "ping",
        "source": my_id,
    }).encode("utf-8")

    _send_multicast(udp, ping_msg)

    local_node = None
    seen_ids: set = set()
    deadline = time.time() + timeout
    resend_at = time.time() + timeout / 2
    resent = False

    while time.time() < deadline:
        if not resent and time.time() >= resend_at:
            _send_multicast(udp, ping_msg)
            resent = True
        try:
            data, addr = udp.recvfrom(65536)
            msg = json.loads(data.decode("utf-8"))
            if msg.get("magic") != _PROTOCOL_MAGIC or msg.get("type") != "pong":
                continue
            node_id = msg.get("source", "")
            if node_id in seen_ids or node_id == my_id:
                continue
            seen_ids.add(node_id)

            node_data = msg.get("data", {})
            machine = node_data.get("machine", "unknown")
            if machine.upper() != local_hostname:
                continue

            local_node = {
                "node_id": node_id,
                "machine": machine,
                "user": node_data.get("user", "unknown"),
                "engine_version": node_data.get("engine_version", "unknown"),
                "ip": addr[0],
            }
            break
        except socket.timeout:
            continue
        except Exception:
            continue

    udp.close()
    return local_node


def _wait_tcp_connection(
    tcp_server: socket.socket,
    udp: socket.socket,
    open_msg: bytes,
    timeout: int = 15,
    retry_interval: int = 2,
) -> Optional[socket.socket]:
    """等待 UE 编辑器反连 TCP，期间定期重发 open_connection 提高成功率。"""
    tcp_server.settimeout(retry_interval)
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            _send_multicast(udp, open_msg)
        except Exception:
            pass
        try:
            tcp_conn, _addr = tcp_server.accept()
            tcp_conn.settimeout(_DEFAULT_READ_TIMEOUT)
            return tcp_conn
        except socket.timeout:
            continue
    return None


def _send_command_tcp(
    tcp: socket.socket,
    my_id: str,
    dest_id: str,
    code: str,
    read_timeout: float = _DEFAULT_READ_TIMEOUT,
) -> Tuple[Dict[str, Any], bool]:
    """通过 TCP 发送 Python 代码并等待执行结果。

    返回 (result_dict, ok_flag)。ok_flag=False 表示连接断开。
    """
    try:
        tcp.settimeout(read_timeout)
    except OSError:
        pass

    msg = json.dumps({
        "version": _PROTOCOL_VERSION,
        "magic": _PROTOCOL_MAGIC,
        "type": "command",
        "source": my_id,
        "dest": dest_id,
        "data": {
            "command": code,
            "unattended": True,
            "exec_mode": "ExecuteFile",
        },
    }, ensure_ascii=False).encode("utf-8")

    try:
        tcp.sendall(msg)
    except OSError:
        return {}, False

    buf = b""
    deadline = time.time() + read_timeout
    while time.time() < deadline:
        try:
            chunk = tcp.recv(65536)
            if not chunk:
                return {}, False
            buf += chunk
            try:
                resp = json.loads(buf.decode("utf-8"))
                if resp.get("type") == "command_result":
                    return resp.get("data", {}), True
                buf = b""
            except json.JSONDecodeError:
                continue
        except socket.timeout:
            if buf:
                continue
            break
        except OSError:
            return {}, False

    return {}, False


def _disconnect(udp: socket.socket, my_id: str, dest_id: str) -> None:
    """发送 close_connection 通知 UE 编辑器断开。"""
    try:
        msg = json.dumps({
            "version": _PROTOCOL_VERSION,
            "magic": _PROTOCOL_MAGIC,
            "type": "close_connection",
            "source": my_id,
            "dest": dest_id,
        }).encode("utf-8")
        _send_multicast(udp, msg)
    except Exception:
        pass


# ====================================================================
# 长连接会话
# ====================================================================


class _UESession:
    """进程级 UE Remote Execution 长连接会话。"""

    __slots__ = ("my_id", "node", "tcp_conn", "tcp_server", "udp_sender", "alive")

    def __init__(self) -> None:
        self.my_id: str = ""
        self.node: Optional[Dict[str, Any]] = None
        self.tcp_conn: Optional[socket.socket] = None
        self.tcp_server: Optional[socket.socket] = None
        self.udp_sender: Optional[socket.socket] = None
        self.alive: bool = False

    def is_alive(self) -> bool:
        """轻量探活：判断 TCP 是否未被对端关闭。"""
        if not self.alive or self.tcp_conn is None:
            return False
        try:
            self.tcp_conn.setblocking(False)
            try:
                data = self.tcp_conn.recv(1, socket.MSG_PEEK)
                if data == b"":
                    return False
            except BlockingIOError:
                return True
            except OSError:
                return False
            finally:
                try:
                    self.tcp_conn.settimeout(_DEFAULT_READ_TIMEOUT)
                except OSError:
                    pass
            return True
        except Exception:
            return False

    def open(self, timeout: float = 6.0) -> bool:
        """完整执行 扫描 → 监听 → 反连。成功返回 True。"""
        self.my_id = str(uuid.uuid4())

        node = _scan_local_node(timeout=timeout)
        if not node:
            return False
        self.node = node

        port = _find_free_port()
        if not port:
            return False

        tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        tcp_server.bind((_COMMAND_IP, port))
        tcp_server.listen(1)
        self.tcp_server = tcp_server

        udp = _create_udp_sender(ttl=_MULTICAST_TTL)
        self.udp_sender = udp

        open_msg = json.dumps({
            "version": _PROTOCOL_VERSION,
            "magic": _PROTOCOL_MAGIC,
            "type": "open_connection",
            "source": self.my_id,
            "dest": node["node_id"],
            "data": {"command_ip": _COMMAND_IP, "command_port": port},
        }).encode("utf-8")

        tcp_conn = _wait_tcp_connection(tcp_server, udp, open_msg, timeout=15)
        if not tcp_conn:
            self._cleanup_sockets()
            return False

        self.tcp_conn = tcp_conn
        self.alive = True
        return True

    def send(
        self, python_code: str, read_timeout: float = _DEFAULT_READ_TIMEOUT
    ) -> Tuple[Dict[str, Any], bool]:
        """在已建立的会话上发送 Python 代码并接收响应。"""
        if not self.alive or self.tcp_conn is None or self.node is None:
            return {}, False
        return _send_command_tcp(
            self.tcp_conn,
            self.my_id,
            self.node["node_id"],
            python_code,
            read_timeout=read_timeout,
        )

    def close(self) -> None:
        """关闭会话。"""
        if self.alive and self.udp_sender is not None and self.node is not None:
            _disconnect(self.udp_sender, self.my_id, self.node["node_id"])
        self._cleanup_sockets()
        self.alive = False

    def _cleanup_sockets(self) -> None:
        for attr in ("tcp_conn", "tcp_server", "udp_sender"):
            s = getattr(self, attr, None)
            if s is not None:
                try:
                    s.close()
                except Exception:
                    pass
            setattr(self, attr, None)


# 进程级单例
_GLOBAL_SESSION = _UESession()


def _ensure_session(timeout: float = 6.0) -> bool:
    """确保全局会话可用。已断则重建。"""
    if _GLOBAL_SESSION.is_alive():
        return True
    _GLOBAL_SESSION.close()
    return _GLOBAL_SESSION.open(timeout=timeout)


# ====================================================================
# 公开 API
# ====================================================================


def send_command(
    python_code: str,
    timeout: float = 6.0,
    read_timeout: float = _DEFAULT_READ_TIMEOUT,
) -> Dict[str, Any]:
    """向本机运行中的 UE 编辑器发送 Python 代码并执行。

    Args:
        python_code: 要在 UE 编辑器中执行的 Python 代码字符串。
        timeout: 扫描 UE 节点的超时时间（秒），默认 6 秒。
        read_timeout: 等待 UE 执行完成并回包的超时（秒），默认 120。

    Returns:
        UE 返回的 command_result 数据字典，包含 output 等字段。
        执行失败时返回含 error 键的字典。
    """
    if not _ensure_session(timeout=timeout):
        return {
            "error": "无法连接到 UE 编辑器。请确认编辑器已启动且启用了 Python Remote Execution。"
        }

    result, ok = _GLOBAL_SESSION.send(python_code, read_timeout=read_timeout)
    if ok:
        return result

    # 失败：重建会话再试一次
    _GLOBAL_SESSION.close()
    if not _ensure_session(timeout=timeout):
        return {"error": "UE 连接已断开且无法重建。"}

    result, ok = _GLOBAL_SESSION.send(python_code, read_timeout=read_timeout)
    if ok:
        return result

    _GLOBAL_SESSION.close()
    return {"error": "命令执行失败，UE 编辑器未响应。"}


def close_session() -> None:
    """显式关闭全局会话。"""
    _GLOBAL_SESSION.close()


def is_connected() -> bool:
    """检查当前是否有活跃的 UE 连接。"""
    return _GLOBAL_SESSION.is_alive()


def get_connection_info() -> Dict[str, Any]:
    """获取当前连接信息。"""
    if not _GLOBAL_SESSION.is_alive() or _GLOBAL_SESSION.node is None:
        return {"connected": False}
    return {
        "connected": True,
        "machine": _GLOBAL_SESSION.node.get("machine", "unknown"),
        "user": _GLOBAL_SESSION.node.get("user", "unknown"),
        "engine_version": _GLOBAL_SESSION.node.get("engine_version", "unknown"),
    }


# 进程退出时自动清理
atexit.register(close_session)
