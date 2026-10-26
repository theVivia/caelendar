[2026-10-26 23:52] 系统托盘故障排查状态

症状：所有进程 Shell_NotifyIcon(NIM_ADD) 返回 FALSE + err=5 (ACCESS_DENIED)
     星历托盘图标挂不上 → 关闭窗口曾静默隐藏（3.39 已改为保持窗口显示）

已排除：
- 星历代码问题（独立 .NET 程序同样失败）
- 权限问题（管理员/最低权限/新用户均失败）
- 火绒（HipsTray 已退，HipsDaemon 杀不掉；驱动 9/21 更新，早于故障）
- explorer 重启、ShellExperienceHost 重启、任务栏对齐切换
- 组策略托盘限制、快捷方式管理员标志（已清除）
- Windows 更新历史（10-25 无更新记录）

时间线：10-24 正常 → 10-25 08:06 起全部失败（同一 exe 3.36）

已做修复：
- 3.39 代码：托盘失败时关闭窗口不再隐藏（Toast 提示）
- 临时恢复 Windows Update 策略（NoAutoUpdate=0，原值 1，备份在 logs\wu-policy-backup.json）
- DISM RestoreHealth 跑到 62.6% 卡住（WU 下载不动），已停止

待办：
1. 重启电脑后测试托盘（DISM 已修复部分组件）
2. 若仍失败：卸载火绒后测试 / 下载 Windows 11 ISO 离线修复 / 系统还原
[2026-10-26 23:56] 重启前状态

- DISM 卡在 64.4%（Windows Update 下载不动），已停止
- Windows Update 策略已恢复为用户原值（NoAutoUpdate=1）
- 已部署开机自检：登录后自动跑托盘测试 + 启动星历，结果写 logs\after-reboot-check.txt
- 3.39 已安装（根目录 星历.exe），关闭窗口不丢程序

下一步：重启后看 logs\after-reboot-check.txt：
- 若独立测试 ok=True → 系统托盘恢复，星历托盘应该正常
- 若仍 err=5 → 需要卸载火绒测试 / 离线修复 / 系统还原
