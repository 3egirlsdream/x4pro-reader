# X4 Pro 返回触控与导航修复报告

交付版本：`1.6.5-weather.0.2.0-back.1`；日期：2026-10-20。此版本已完成主机回归和 X4 Pro 固件构建，**尚未刷入阅读器，也没有实机触摸、墨水屏延迟或唤醒验收结果**。

## 源码依据与隔离

- CrossPoint HEAD：`93e98bb78702e29868a16a13b80c40e6b36ccdff`。
- FreeInk SDK：`111fdcc7f0176c3ee38391a160ee296bf492dbd8`，子模块源码无修改；Lucide：`c81680e066f45b640743ca78ae36cdedda3f0318`。
- 原天气扩展补丁 SHA256：`9a390cca3e94531ab7d77cf3c90b66401769fcfc33cce2964f46342473b05754`。开始工作时逐文件校验 source-files.sha256 全部通过，原仓库 `git diff --binary HEAD` 的哈希与补丁一致。
- 本目录由本地无硬链接克隆基线、应用完整天气补丁、独立克隆两个子模块建立。未修改原仓库、桌面或游戏会话文件；没有提交、推送、跨会话消息或设备访问。

## 确认的根因

| 类型 | 生产代码证据（修复前） | 复现序列 | 修复 |
|---|---|---|---|
| 真正 BUG | `KeyboardEntryActivity::loop` 在键盘路由后对所有 `pressedDown || tapped` 提前返回，Back 在函数末尾 | 密码、SSID、服务器参数、经纬度或地点名称键盘点顶部箭头，无键命中仍提前返回 | Back 在键盘/光标输入前处理；取消结果仍由原 `onCancel()` 产生 |
| 真正 BUG | `WifiSelectionActivity::loop` 的 CONNECTING/AUTO_CONNECTING 分支只有 AUTO_CONNECTING 判断 Back | 手动连接网络时点箭头/按返回，必须等超时；自动连接却能取消 | 两种连接状态共用 disconnect + `onComplete(false)`；回到原父页面 |
| 真正 BUG | `OptionPopup::handleInput` 先处理原始抬起并无条件 return，后面才检查 Back | 左边缘右滑产生 -1,-1 的非点按抬起，被吞掉；对话框无法手势取消 | Back 先于通用触摸，关闭最上层弹窗一次；不执行确认回调 |
| 真正 BUG | `ClearCacheActivity::loop` 中弹窗消失后仍等待新的实体键 | 返回/点外部关闭确认框，停在无弹窗的缓存警告页 | 弹窗取消后结束 Activity，不清理缓存 |
| 漏接路由 | `ButtonRemapActivity` 只读原始前键和侧键，尽管绘制了顶部 Back | 点箭头无反应 | 屏幕 Back/边缘返回取消；原始实体前键仍用来配置映射 |
| 触区/一致性 | `BaseTheme::drawHeader` 手动存 48×48；书库/OPDS 用 SDK interaction table 的另一条路 | 箭头附近触摸结果随页面变化，普通标题不享有 SDK 的最小触区扩展 | 三种标题入口共用生产 geometry 与 MappedInputManager 返回路径；不再注册第二个 Back action 回调 |
| 返回层级风险 | SDK 点按可持续超过 1s，`rememberTouchHeldTime` 把触摸时间传给 FileBrowser/阅读导航长按判定 | 缓慢点按箭头被当作实体键长按，跳到根目录或其他目标 | 屏幕返回/边缘返回的逻辑 held time 为 0；实体键长按保留 |
| 生命周期风险 | HeaderBackTapTarget 四个普通 int 跨核读写；切页虽清矩形却不取消当前接触；pending action 前仍会调用旧页面 loop | 刷新读到混合矩形；按住时切页再抬起；全局导航请求在下一轮旧页面再次收输入 | 32bit 原子发布；每帧 staging；切页取消当前接触并屏蔽被按住的实体键到其释放；pending action 优先 |
| 交互语义 | 设置和书库先把焦点移回标签栏，画面大部分不变 | 顶部箭头第一次看似无效，第二次才退出 | 屏幕 Back 跳过纯焦点退回；书库搜索清除/分组展开仍保留，实体键仍可回到标签栏 |
| 显示误导 | 固件写入、USB 独占、同步请求忙碌时仍画可点箭头 | 箭头可见但流程不能退出 | 这些状态隐藏箭头并移除触区；保留其原有执行和退出规则 |

源码实际入口：`src/MappedInputManager.cpp`、`src/components/HeaderBackTapTarget.h`、`src/util/NavigationTouch.h`、`src/activities/ActivityManager.cpp`。页面差异见 [页面矩阵](page-matrix.md)。修复没有增加导航路径上的堆分配、按下重绘或 SD 日志。

## 触摸与渲染协议

1. 按下时在主循环记录该接触的起点、当时方向、已发布的返回目标。每次采样检查与起点的最大位移，任一轴超过 28px 即永久取消这一接触的返回资格，拖回原处也不恢复。
2. 抬起还必须通过固定 SDK 的单指点按分类；多点、滑动及 SDK 已抑制的接触不能返回。屏幕返回允许慢点按至 1199ms；≥1200ms 视为保持，不触发返回。500ms 的 SDK 长按阈值继续用于支持长按的列表/阅读内容；返回按钮不借此产生实体长按含义。
3. SDK 的点按坐标是按下点；X4 Pro GT911 抬起帧不含新接触坐标。最大位移来自此前所有主循环可见的 live 样本。没有改动控制器校准、原始 I²C 采样、SDK 59px tap-release/60px swipe 分类或阅读区手势规则。
4. 目标围绕实际绘制的 48px 方框扩大，宽 64px，高度最多 64px，裁剪于标题内容行，避开电池/时钟状态栏和下一行。典型光学偏移 5px 时 Base/Lyra/Lyra3 是 64×59，RoundedRaff 是 64×57。没有强行扩大到覆盖相邻控件的 64×64。
5. 主题标题、书库、OPDS 都写入 render-task staging。ActivityManager 在完整 render 返回后一次原子发布；普通重绘期间旧的完整触区保持可用。没有标题的整帧发布空目标；隐藏箭头也发布空目标。
6. 首帧发布前的接触不追认成新箭头点击。刷新中相同目标允许按下/抬起跨帧；目标改变或方向改变则取消。旧页面的触摸不转交给新页面。
7. Back 查询在同一输入帧内保持稳定，允许 FileBrowser 等先检查长按再检查短按；真正调度 push/pop/replace 时同步抑制本帧及当前接触。不能简单把 `wasReleased` 改为查询即消费，否则前面的长按探测会吞掉短按。
8. 页面内 Cancel/OK 保持 SDK 的真实 interaction table 路由和控件布局；本次没有全局放大所有按钮。天气底部绘制和命中使用同一个 `WeatherTouchLayout.h`，按钮高 50px，中央 16px 间隔和侧边空白不再触发返回/刷新。

48 个主机布局组合来自四主题 × 四方向 × 三个字体光学度量边界。0/5/12px 为度量样本，不是宣称三个设备字号恰好等于这些值。安全区样本额外覆盖横屏偏移。SDK header 真实绘制调用输出的 32px 图标矩形被断言完全位于返回触区内。

## 已执行验证

- 新增 `scripts/test_back_navigation.py`：101,349 次断言通过（其中 100,000 次为原子发布并发读取检查，不能当作 100,000 个不同使用场景），UBSan 无报错。
- 编译实际生产 `MappedInputManager.cpp` 全部方法、实际 `GfxRenderer::tapToLogical`、实际 ActivityManager loop/push/pop/replace/exit 与 Activity 生命周期方法；用内存 HAL/任务锁适配器提供输入和环境。
- 编译实际 UiAppHost 方法、touchSnapshotFrom、OptionPopup 输入方法，使用真实 SDK FreeInkApp/InteractionBuffer；覆盖首帧关闭路由、普通重绘跨按下/抬起、显式关闭路由、切页后的旧事件。
- 编译键盘输入函数的前置取消段、Wi-Fi 连接状态分支、清缓存完整 loop、设置/书库返回分支、USB 完整 loop、SD 更新完整 loop。并非把完整 Wi-Fi/网络栈或键盘硬件模拟成实机。
- 覆盖边界内外、微移、拖离再回来、慢点按、保持、方向变化、隐藏/首帧、接触跨切页、旧实体键释放、父页面状态及 result 一次调用、pending action 跳过旧页面、USB 独占不返回、固件写入不退出/失败可退出。
- 原有 CTest：378/378；SDK FreeInkUI：226,359 checks / 0 failed；天气模型/缓存/存储/低电量策略、TXT 按需分页、列表 viewport 回归通过。
- X4 Pro 专用目标 `x4pro-gh_release` 构建通过。链接 Flash 5,894,766 bytes（相对 0.2.0 +1,524）；静态 RAM 102,744 bytes（+16）。应用分区仍为 6,553,600 bytes，未改变 partitions.csv。应用镜像实际字节数和 SHA256 见固件清单。
- C++ 格式仅运行仓库 `./bin/clang-format-fix -g`。使用本会话隔离安装的 21.1.8；没有修改共享工具环境。源码与原基线的差分检查通过。

主机代码与 SDK 检查可证明逻辑不变量，不能证明触摸硬件丢包率、玻璃边缘手感、真实响应毫秒数、长时耗电或休眠唤醒稳定性。

## 时延与未解决的硬件/底座限制

- 输入分类和命中不要求刷新。返回调度沿用原 Activity RenderLock；若面板刷新仍持有锁，切页要等正在进行的刷新完成。没有添加按下全刷，不能将其描述为实机“零延迟返回”。
- 既有同步 NTP、认证、OTA 元数据查询和部分 OPDS 网络操作仍会暂时阻塞主循环。本次隐藏这些不接收返回的忙碌箭头，未把网络子系统改造成可任意时刻取消的异步任务；这属于明确的残余交互限制，并非 Flash/USB 那样的安全必要禁返。需在实机记录最坏等待时间。
- 触控在首个画面尚未发布时不激活标题返回，避免按在旧墨水画面却触发新页面。可在首帧完成后重新点按。
- 主机测试不使用 SD 卡、真实屏幕波形、GT911、中断、电源休眠或真实网络。0.2.0 长篇阅读、天气刷新及息屏原有实测仍是待完成；本版本没有继承为“通过”。

## 集成与交付

[集成说明](integration.md) / [中文上机步骤](hardware-validation.md)。固件目录仅把 application bin 作为刷入候选，不交付自动刷机脚本。交互预览、日志、几何 CSV 和构建清单位于旁边的 evidence / firmware 目录。
