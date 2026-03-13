# Battle
在 UE5.5 中，Quinn 使用的是虚幻引擎最新的 SKM_Manny 标准骨架体系。这套骨架相比 UE4 时代复杂得多，主要是为了支持更高级的过程动画（Procedural Animation）和物理模拟。
根据你提供的列表，这套骨架可以分为以下四大核心部分：
1. 核心变形骨骼 (Core Deformation Bones)
这是最基础的骨架，决定角色的基本位移和旋转。
root / pelvis: 根骨骼与盆骨，动画的起始点。
spine_01 ~ 05: 脊柱，UE5 增加到了 5 节（UE4 为 3 节），能实现更细腻的弯腰和扭转效果。
clavicle (锁骨) / thigh (大腿) / calf (小腿) / foot (脚): 标准的四肢结构。
2. 肌肉与体积修正骨骼 (Corrective & Helper Bones)
这是你图中数量最多的部分，也是 Quinn 动作自然的关键：
clavicle_pec / spine_04_latissimus: 正如之前提到的，这些骨骼模拟胸大肌和背阔肌。当手臂抬起时，它们会自动移动以支撑皮肤，防止肩膀处塌陷。
thigh_fwd / bck / in / out: 大腿根部的辅助骨骼。在做出劈叉或高踢腿动作时，这些骨骼会撑起臀部和大腿根部的形状。
calf_knee / calf_kneeBack: 膝盖处的修正骨骼。当膝盖深度弯曲时，它们会“顶出”关节，防止膝盖模型像软糖一样缩成一团。
3. 扭转修正骨骼 (Twist Bones)
thigh_twist / calf_twist: 也就是我们常说的“扭转骨骼”。
当大腿或小腿旋转时，如果只有一根骨骼，皮肤会像拧麻花一样缩水。Twist 骨骼通过分摊旋转权重，让肌肉旋转看起来非常平滑。
4. 逻辑与 IK 骨骼 (Logic & IK Bones)
ik_foot_root / ik_hand_root: 这些骨骼在模型上没有权重，主要用于 IK（反向动力学） 结算。比如脚部吸附地面、手部抓握武器等逻辑。
ik_hand_gun: 专门用于武器对齐的参考点。
interaction / center_of_mass: 交互参考点和质心点，通常用于物理引擎计算或复杂的交互逻辑（如搬运物体）。
总结
这套骨架的设计哲学是：主骨骼管动作，修正骨骼管颜值。 它是为了配合 UE5 的 ML Deformer（机器学习变形器） 或 Control Rig 而设计的。