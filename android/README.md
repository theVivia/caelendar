# 星历 Android（v1 骨架）

桌面版 CAELENDAR / 星历 的 Android 客户端骨架，与桌面共用 `calendar.json` 数据契约和账号同步 API。

## 当前状态

- [x] **已成功构建 APK**（app-debug.apk，约 7.1 MB）
- [x] 工程结构（Kotlin + Jetpack Compose）
- [x] 数据模型（Event / Tag / Store，与桌面 `data/calendar.json` 对齐）
- [x] 本地存储（`filesDir/calendar.json`）
- [x] 月视图 + 日程列表
- [x] 简单新建/编辑（标题 / 日期 / 开始结束时间 / DDL / 完成）
- [x] 账号同步客户端（登录 / 上传 / 拉取，对接桌面内置同步服务）
- [x] 到点提醒通知（提前 10 分钟，AlarmManager + 系统通知，点击打开应用；开机后自动重排）
- [x] DDL / 任务页（未完成任务按截止排序，点行标记完成）
- [x] 成绩 / GPA 页（4.0 制学分加权，存 filesDir/grades.json，与桌面格式一致）
- [x] 同步设置持久化（服务器/账号/token 存 SharedPreferences，重启不用重填）
- [x] onResume 重排提醒（新建/修改日程后回到应用即刷新下一场提醒）
- [x] 已用 Android 模拟器真机验证：月视图/列表/任务/GPA/新建日程（含 DDL）/GPA 录入/账号同步端到端
- [x] 修复：明文 HTTP 被 Android 9+ 拦截（usesCleartextTraffic）；同步网络调用移到 IO 线程（原主线程 NetworkOnMainThreadException）
- [x] 提醒通知实测：AlarmManager 到点触发 → 系统通知栏弹出「⏰ NotifyTest 15:34」（截图 shots/android-notification2.png）
- [x] 桌面小组件 v1：今日日程数 + 下一场时间；模拟器桌面添加并渲染成功（截图 shots/android-widget-step9.png）
- [x] 通知点击定位：提醒通知携带事件日期，点击打开月视图并选中该日期（onCreate/onNewIntent 均实现；模拟器 am --es date 验证 onCreate 路径）
- [x] GPA 数据跨端同步：上传/拉取携带 grades 数组；模拟器端到端验证（上传后服务端 JSON 含成绩，拉取状态显示「已拉取 2 条日程、1 门成绩」，GPA 保留）
- [x] 提醒通知（AlarmManager + 开机重排）
- [x] DDL / 任务页
- [ ] 完整重复规则编辑
- [x] 打包 APK（debug 包已产出）

## 构建前提（本机已配置，SDK 与 Gradle 缓存已移到 E 盘）

本机环境：

- JDK 17（`C:\Program Files\Eclipse Adoptium\jdk-17.0.15.6-hotspot`）
- Gradle 7.6（`C:\Gradle\gradle-7.6`）
- Android SDK：`E:\AndroidSdk`（platform 33、build-tools 33.0.2、platform-tools）
- Gradle 用户缓存：`E:\GradleHome`（`GRADLE_USER_HOME`）

构建命令（PowerShell）：

```powershell
cd E:\myCalendar\android
$env:ANDROID_HOME='E:\AndroidSdk'
$env:ANDROID_SDK_ROOT='E:\AndroidSdk'
$env:GRADLE_USER_HOME='E:\GradleHome'
& 'C:\Gradle\gradle-7.6\bin\gradle.bat' --no-daemon assembleDebug
```

产物：
- debug：`android/app/build/outputs/apk/debug/app-debug.apk`
- **release（签名）**：`android/app/build/outputs/apk/release/app-release.apk`，交付副本 `E:\myCalendar\星历-Android-v13-release.apk`
- 构建：`gradle --offline assembleRelease`（已配置 signingConfigs；release lint 已关闭避免离线依赖缺失）

## 跨日分段实线/虚线（2.45.0）

- 周/日视图跨日事件按天分段：起始日实线、后续日虚线；列表视图显示跨日区间

## 拖动门槛 2 秒（2.44.4）

- 周/日视图事件块拖动改为长按 2 秒后生效；短按拖动无效、单击打开编辑

## 字体行距修复（2.44.3）

- 周/日视图事件块字号行高改 dp 固定，不随系统字体缩放

## 四项反馈修复（2.44.2）

- 清空日程、跨日分割式、拖拽改长按后拖、列表长按进多选

## SQLite 引擎（2.43.0）

- 设置页存储引擎切换：JSON ↔ SQLite（自动迁移，读库写双写 JSON）

## 长按多选 + 局域网分享（2.42.0）

- 长按事件条进入多选（批量完成/删除/加标签）；数据区「局域网分享」手机开只读 HTTP 页

## 自定义强调色（2.39.0）

- 外观「自定义强调色 #RRGGBB」输入 + 应用/清除；无障碍补全部分按钮

## 统计饼图（2.38.0）

- 统计按标签时长环形饼图；标签图标英文名映射 emoji（book→📚 等）

## 性能修复（2.37.0）

- 月视图整月只展开一次、年视图预计算日期集合（修主线程 2.4s 卡顿）
- 冒烟：年视图迷你格/设置页/考试倒计时/存档/标签/编辑对话框全部正常

## 多选批量（2.36.0）

- 月视图「多选」：勾选当日日程，批量标记完成/删除/加标签（全模块对比方案 P0/P1/P2 全部完成）

## 导出与存档（2.35.0）

- 导出未来 7 天学习计划 HTML（分享/打印）、多存档管理（保存/载入/删除）

## 考试倒计时（2.34.0）

- 设置页考试倒计时：未来 90 天考试剩余天数（标题/标签自动识别）

## 拖拽改日期（2.33.0）

- 周/日视图左右拖事件块改日期（重复实例写 override）；统计加本周喝水达标统计

## 主题与年视图（2.32.0）

- 强调色 8 色可选、标签图标点击切换、年视图迷你月历点阵

## 智能输入（2.31.0）

- 编辑对话框智能输入：「明天9点开会 #课程」自动解析日期/时间/标签/标题；标签管理显示计数；统计显示完成率

## 长按与详情（2.30.0）

- 长按事件条：编辑/复制/删除；复制后长按日期粘贴；当日详情对话框；周/日视图重叠事件并排

## 月视图增强（2.29.0）

- 月视图事件条 3 条、公历节日表（含母亲节/父亲节/感恩节）、周/日视图当前时间线

## 批次 A：编辑补全 + 喝水（2.28.0）

- 编辑对话框：全天事件、跨天结束日期、URL、描述、状态、提醒响铃/文本、重复星期多选+间隔+农历
- 喝水打卡（月视图 8 杯）+ 喝水提醒（08:00–22:30 每 45 分钟，通知 +1 杯）
- 周/日视图重复系列展开修复；顶栏品牌 logo；Tab「预览」；事件条显示时间

## 顶栏修复（2.27.0）

- 「设置」选项卡被挤出屏幕 → 顶栏紧凑化 + horizontalScroll，5 个 Tab 全部可见可点（大字号可横滑）

## UI 修复：logo 与按钮布局（2.26.0）

- 顶栏加品牌 logo（ic_launcher_foreground 金色四芒星）+「星历」
- 月视图工具栏改弹性布局：「+ 新建」不再折行成 "+ 新/建"；批量/模板缩小内边距
- 全模块差异对比见 `docs/Android与桌面全模块对比方案.md`

## 同步格式与重复展开修复（2.25.0）

- **修复日程"损失大半"根因**：桌面 recur 是 JSON 对象，旧代码用 RRULE 正则匹配失败 → 重复课程全部不展开；现用 CtRecur 解析 JSON（freq/interval/byDay/interval/until/count/exDates）
- **写回保持桌面格式**：事件保留原始 JSON（url/source/recurId/deleted/overrides），recur 写对象、alarms 写对象数组；Store 保留 version/updatedAt/water/标签 builtin
- **应用 overrides**：改课/取消课按 recurId 替换实例；编辑单次实例写 overrides，不再覆盖整个系列
- **合并对齐桌面**：墓碑按时间比较、water 并集、墓碑上限 300
- **设置页竖屏布局**：数据区按钮改两行，全部可点
- 端到端验证：拉取 80 条真实数据 → 月视图课程密集正确 → 上传后服务器 recur 仍为对象、water/version/builtin 保留

## 机器人 / 教务表 / 性能（2.24.0，P3-2）

- Telegram 机器人：Bot Token/Chat ID 配置、测试连接、发送今日、拉取 /today /week 命令并回复（手动拉取）
- 教务表导入：批量导入对话框新增「按教务表导入」（课程名,周一,3-4节,教室 → 16 周每周事件）
- 性能：列表 ExpandEvents 加 remember 缓存；列表已用 LazyColumn

## 撤销重做与快捷键（2.23.0，P3-1）

- 撤销/重做：编辑保存、删除、拖拽前自动快照（最多 30 步），视图栏显示「↶ 撤销 / ↷ 重做」
- 外接键盘：Ctrl+Z 撤销、Ctrl+Y 重做、Ctrl+N 新建、Ctrl+F 搜索、Ctrl+T 今天
- 事件编辑保存刷新 updated；编辑器颜色扩到 12 色（两行排布）

## 外观与无障碍（2.22.0，P2-3）

- 主题预设 4 套（金色午夜/极简蓝/森林绿/浅色纸张）一键切换
- 字号 4 档（小/标准/大/特大），全局 fontScale 生效
- TalkBack：搜索/新建/月份切换等关键控件加 contentDescription

## 订阅与分享（2.21.0，P2-2）

- ICS URL 订阅：添加订阅地址、立即同步拉取合并（按 id/uid 去重）
- 忙闲分享：生成未来 7 天空闲时段文本并系统分享
- 通知编辑跳转：通知携带 eventId，点击直接打开该事件编辑器

## 导入导出增强（2.20.0，P2-1）

- 批量导入：粘贴文本/CSV/ICS 自动解析（标题,日期,开始,结束,地点）、剪贴板读取、预览条数后导入
- 加密备份：AES-GCM 口令加密导出（CAELENDAR-ENC:v1）
- 立即快照：手动创建 snapshot 备份

## 统计与备份管理（2.19.0，P1-3）

- 统计周期切换：近 7 天 / 近 30 天 / 全部 + 总时长/次数 + 标签条形 + 本周每日柱状
- 备份管理页：列出备份文件（名称/大小/条数）、恢复、删除；保存前自动备份保留最近 10 份

## 全局搜索与命令（2.18.0，P1-2）

- 顶栏「搜索」入口：关键词搜标题/地点/描述/标签/日期，结果点击跳转日期或编辑
- 命令面板：输入 `>` 显示命令（>new、>today、>month/week/day/year、>list、>tasks、>gpa、>settings、>date YYYY-MM-DD）

## 重复与提醒增强（2.17.0，P1-1）

- 重复：每天/每周/每月/每年/工作日 + 结束条件（永不/日期/次数）+ 例外日
- 提醒：多选提醒（准时/5/10/15/30/60/120 分）+ 自定义分钟
- ReminderScheduler 支持多条提醒（每个提前量各调度一条，最多 8 条）

## 跨日完善与颜色（2.16.0，P0-2）

- 跨日条支持日视图；跨周事件按周裁剪显示
- 编辑器新增自定义颜色输入（#RRGGBB）+ 7 色预设
- 月视图日期格显示事件数角标（N 项）
- 拖动灵敏度修正（px→dp 换算）

## 删除与拖拽（2.15.0，P0-1）

- 编辑器底部「删除」按钮 + 二次确认；删除写入 tombstones（与桌面同格式），合并同步时应用墓碑防止复活
- 周/日视图事件块拖动改时间（15 分钟吸附），保存刷新 updated

## 视图交互与颜色（2.14.0）

- 周/日/年视图左右切换（‹ › 按钮 + 左右滑动手势）
- 事件颜色优先级：事件色 → 标签色 → 主题色（周视图事件块同步生效）
- 周视图顶部跨日条
- 设置页显示「本地共 N 条日程」便于同步自检

## 同步合并修复（2.13.0）

- 同步从「整份覆盖」改为「合并」：拉取/上传前先拉服务器数据，按 id + updated 合并后写回，双方独有事件不丢失
- 事件新增 uid/created/updated 字段（与桌面格式一致），编辑保存时刷新 updated
- 验证：模拟器「本地 6 + 服务器 79 → 85 条」，上传后服务器 85 条不丢

## 小组件多模式（2.12.0）

- 设置 → 小组件样式：今日 / 本周 / 时钟三种模式循环切换，切换后立即刷新桌面小组件

## GPA 学期分组 + 数据备份（2.11.0）

- 成绩支持「学期」字段，列表按学期分组显示各组 GPA/学分
- 设置 → 数据：导出备份（JSON 分享）/ 恢复备份（选择 JSON 覆盖）

## 跨日事件条（2.10.0）

- 月视图同一周内跨天事件绘制连续横条（跨越起止列），普通单日事件仍显示在格内

## 批量添加（2.9.0）

- 月视图「批量」：标题 + 开始日期 + 天数（1-30）+ 每天起止时间 + 跳过周末，一键生成多天日程

## 节气 + 高对比度（2.8.0）

- SolarTerms：移植桌面 solarterm.cpp（Meeus 低精度太阳黄经），月视图显示 24 节气（寒露/霜降等金色高亮）
- 高对比度：设置 → 外观开关，文字/日期对比度增强（持久化）

## 农历（2.7.0）

- LunarCalendar：移植桌面 lunar_table.inc（1901–2100 数据表），月视图日期格显示农历日/月份名、传统节日（春节/中秋/重阳等金色高亮）

## 平台特性（2.6.0）

- 应用快捷方式（长按图标）：新建日程 / 回到今天
- 分享接收：其它应用分享文本或 ICS 到星历（text/plain、text/calendar）
- 小组件支持缩放（minResize 110×40dp，支持 2×2 / 4×2 / 4×4）
- TalkBack：顶栏标签加 contentDescription

## GPA 增强（2.5.0）

- 成绩条目支持编辑（课程名/学分/成绩对话框）
- 导出成绩 JSON（系统分享）/ 导入 JSON 覆盖

## 习惯打卡（2.4.0）

- 设置 → 习惯打卡：添加/删除习惯、每日打卡、连续天数、近 7 天点状记录（SharedPreferences）

## 番茄钟（2.3.0）

- 设置 → 番茄钟：15/25/45 分钟选择、开始/暂停/重置、进度条、倒计时
- 完成一场自动记录到 SharedPreferences；显示今日次数与分钟数

## 统计（2.2.0）

- 设置 → 统计：近 7 天总时长、按标签时长条形图、本周每日事件小柱状图

## 提醒操作与提醒中心（2.1.0）

- 通知按钮：「完成」（标记日程完成）/「稍后 10 分钟」（重排提醒）
- 设置 → 提醒中心：未来日期未完成事件列表，一键完成

## 数据与备份（2.0.0）

- ICS 导出：设置 → 数据 → 导出 ICS，生成 caelendar.ics 并调起系统分享
- ICS 导入：文件选择器读取 .ics，解析 VEVENT（标题/时间/地点/描述/颜色）后追加到日程
- 自动备份：每次保存前备份 calendar.json 到 backup/auto-*.json，保留最近 10 份

## 日程模板（1.9.0）

- 编辑器底部「存为模板」：保存标题/地点/起止/优先级/颜色/提醒/重复
- 月视图「模板」按钮：模板对话框（列表 + 删除 + 关闭），点模板套用到选中日期并打开编辑器

## 重复规则（1.8.0+）

- 列表项显示日期（MM-dd 时间），重复展开后的 occurrence 可区分

- 编辑器「重复」：不重复 / 每天 / 每周 / 每月（存 recurJson，RRULE 风格字符串，与桌面 recur 字段兼容）
- 列表页展开未来 8 周的重复 occurrence；月视图事件条按重复规则显示到对应日期
- 编辑器底部按钮固定（保存/取消始终可见，字段区可滚动）

## 提醒提前量（1.7.0）

- CtEvent 新增 `alarms` 字段（分钟数组，与桌面 format 2 对齐）
- 编辑对话框「提醒」：无 / 准时 / 5 / 10 / 15 / 30 / 60 分钟
- ReminderScheduler 按事件 alarms 首个值计算提醒时刻（默认 10 分钟）

## 阶段 C：管理功能（1.6.0）

- 日程列表：搜索标题/地点 + 筛选（全部/今天/未来7天/未完成/已完成）
- 标签管理：设置页 → 标签管理（查看/删除标签，新增自定义标签 + 7 色板）
- 冲突检测：编辑日程时若与同日期已有日程时间重叠，显示红色警告
- 关于页版本号改为读取 BuildConfig.VERSION_NAME

## 日程编辑增强（1.5.0）

- 编辑对话框新增：地点、优先级（无/低/中/高）、颜色（7 色板）、标签多选（横向滚动 chips）
- 月视图事件条支持点击直接编辑

## 视图（1.4.0+）

- 年视图：12 个月卡片 + 事件密度热力条，点击进入月视图
- 任务页：DDL 两列看板（未完成 / 已完成），点未完成卡片标记完成

- 月视图事件条：日期格内显示最多 2 条彩色事件条（标题截断）+ 超出 "+N"，事件颜色取 color 字段或主题色

- 月视图顶部「月 / 周 / 日」切换
- 周视图：周一~周日 7 列时间轴（00:00–24:00，每小时 36dp），事件块按时间定位，点击时间格新建、点击事件块编辑
- 日视图：单日时间轴（同组件单列）

## 主题与启动页（1.1.0）

- 启动页：`res/drawable/splash_bg.xml`（深色 + 品牌图标居中）+ `Theme.Caelendar.Splash`
- 主题：设置 → 外观，深色/浅色切换 + 3 种强调色（金/蓝/绿），SharedPreferences 持久化
- 关于页：设置页底部（版本 1.1.0、数据格式 format 2）

## 应用图标

- 由桌面品牌图标（`res\xingli.ico`，深色方块 + 金色四芒星）生成全套 mipmap（mdpi–xxxhdpi）
- 自适应图标：`mipmap-anydpi-v26/ic_launcher.xml` + `ic_launcher_round.xml`（背景色 #0B0D14）
- 桌面启动器实测已显示品牌图标（截图 shots/android-drawer2.png）

## 签名信息

- keystore：`android/keystore/caelendar-release.jks`（不要公开上传）
- alias：`caelendar`；store/key 密码见本机 `android/keystore.properties`（该文件不入库）
- 证书：CN=Caelendar；SHA-256 `42fd0954...4b4cf3a`
- versionName 1.0.0 / versionCode 2（升级时需递增 versionCode）

## 模拟器验证环境

- 模拟器与系统镜像：`E:\AndroidSdk\emulator`、`system-images;android-33;google_apis;x86_64`
- AVD：`E:\AndroidAvd\caelendar_test.avd`（4 核 / 4 GB 内存，WHPX 加速）
- 启动：`E:\AndroidSdk\emulator\emulator.exe -avd caelendar_test -no-window -gpu swiftshader_indirect -memory 4096 -cores 4`
- 关闭：`E:\AndroidSdk\platform-tools\adb.exe emu kill`

> 注意：SDK 和 Gradle 缓存都放在 E 盘，不要放回 C 盘。`local.properties` 里 sdk.dir 指向 `E:\AndroidSdk`。

`compileSdk = 33`，`minSdk = 26`（使用 java.time），AGP 7.4.2 + Kotlin 1.8.10 + Compose 1.4.2。

## 与桌面端的关系

- 数据文件格式完全一致：`data/calendar.json`（format 2）
- 同步走桌面端「账号同步」的 HTTP API：
  - `POST /api/auth/register`、`POST /api/auth/login`
  - `POST /api/sync/push?token=…`、`GET /api/sync/pull?token=…`
- 桌面端需先开启分享/同步服务（默认端口 17890）；手机与电脑在同一局域网，或通过 Tailscale / 反向代理访问。

## 目录

```
android/
  settings.gradle.kts
  build.gradle.kts
  gradle.properties
  app/build.gradle.kts
  app/src/main/AndroidManifest.xml
  app/src/main/java/com/caelendar/app/MainActivity.kt
  app/src/main/java/com/caelendar/app/data/CalendarModels.kt
  app/src/main/java/com/caelendar/app/data/CalendarRepository.kt
  app/src/main/java/com/caelendar/app/data/SyncClient.kt
```
