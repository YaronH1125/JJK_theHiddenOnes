# 原创设计示意图；非动画截图。运行后在本目录生成两张 PNG。
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$script:owned = [System.Collections.Generic.List[System.IDisposable]]::new()
function B([string]$hex) { $v=[System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml($hex)); $script:owned.Add($v); return $v }
function P([string]$hex,[float]$width=2) { $v=[System.Drawing.Pen]::new([System.Drawing.ColorTranslator]::FromHtml($hex),$width); $script:owned.Add($v); return $v }
function T([string]$text,[float]$x,[float]$y,[float]$size=24,[string]$color='#e9eef6') {
    $font=[System.Drawing.Font]::new('Microsoft YaHei',$size,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
    $script:g.DrawString($text,$font,(B $color),$x,$y); $font.Dispose()
}
function Box([float]$x,[float]$y,[float]$w,[float]$h,[string]$color) {
    $script:g.FillRectangle((B '#1c2b40'),$x,$y,$w,$h)
    $script:g.DrawRectangle((P $color 2),$x,$y,$w,$h)
}
function Arrow([float]$x,[float]$y,[float]$x2,[float]$y2,[string]$color='#86a8ca') {
    $pen=P $color 3; $pen.EndCap=[System.Drawing.Drawing2D.LineCap]::ArrowAnchor
    $script:g.DrawLine($pen,$x,$y,$x2,$y2)
}
function Start-DesignCanvas([int]$height) {
    $script:bitmap=[System.Drawing.Bitmap]::new(1500,$height)
    $script:g=[System.Drawing.Graphics]::FromImage($script:bitmap)
    $script:g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $script:g.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $script:g.Clear([System.Drawing.ColorTranslator]::FromHtml('#101a2b'))
}
function Save([string]$name) {
    $script:bitmap.Save((Join-Path $PSScriptRoot $name),[System.Drawing.Imaging.ImageFormat]::Png)
    $script:g.Dispose(); $script:bitmap.Dispose()
    foreach($resource in $script:owned){$resource.Dispose()}; $script:owned.Clear()
}
Start-DesignCanvas 900
T '三人领域意象 · 证据与推断' 55 38 40
T '刀光 / 烟火 / 青白火焰来自报道；下方对应关系为资料推理，非官方定论。' 58 99 23 '#aebfd4'
$xs=@(60,555,1050)
$titles=@('刀身光芒','放射烟火','青白火焰 / 飘带意象')
$people=@('乙骨忧太','石流龙','乌鹭亨子')
$colors=@('#a4c7ee','#edbd7b','#82d4cf')
for($i=0;$i -lt 3;$i++) {
    Box $xs[$i] 176 390 110 $colors[$i]
    T $titles[$i] ($xs[$i]+23) 210 28 $colors[$i]
    Arrow ($xs[$i]+195) 290 ($xs[$i]+195) 341 $colors[$i]
    Box $xs[$i] 346 390 328 $colors[$i]
    T $people[$i] ($xs[$i]+23) 373 34 $colors[$i]
}
T '后期「真贋相愛」' 83 438 25
T '刀阵与复制术式关联' 83 480 24
T '支持刀光对应乙骨' 83 522 24
T '对应证据相对最强' 83 610 23 '#a4c7ee'
T '高输出：聚集 → 放出 → 爆发' 578 438 23
T '追求满足的战斗体验' 578 480 24
T '烟火契合力量与情绪' 578 522 24
T '本项目采用：合理推断' 578 610 23 '#edbd7b'
T '官方设定：把「空」视作面' 1073 438 22
T '流动、包裹与形象可呼应' 1073 480 23
T '颜色不能证明火焰术式' 1073 522 23
T '暂定对应：把握较低' 1073 610 23 '#82d4cf'
T '排除乙骨后仍有两种排列；人物契合与爱好者整理增强推断，不能替代制作组确认。' 64 726 25
T '设计选择：烟火是咒力的视觉元素，不新增火属性、灼烧或熔岩系统。' 64 775 25 '#edbd7b'
T '原创示意 · S1 动画报道 / S2 官方角色图 / S4—S6 资料对照 · 出处见 Docs/08' 64 849 19 '#91a5c0'
Save 'ishigori-domain-evidence.png'
Start-DesignCanvas 1000
T '石流龙 · 双形态战斗循环' 55 38 40
T '左键 = 基础攻击    Q = 当前形态技能    E = 切换形态    R = 领域展开' 58 100 25 '#aebfd4'
Box 60 183 510 295 '#edbd7b'
T '近战形态' 87 207 32 '#edbd7b'
T '左键：拳击 / 长按重拳' 87 265 25
T 'Q：腿击 / 长按重踢' 87 310 25
T '拳脚不耗咒力 · 有效命中少量恢复' 87 365 24
T '抓到机会 → 击退 / 合法取消' 87 418 24 '#aebfd4'
Box 930 183 510 295 '#86ccef'
T '远程形态' 957 207 32 '#86ccef'
T '左键：可移动蓄力炮' 957 265 25
T 'Q：定点超级蓄力炮' 957 310 25
T '伤害随蓄力变化 · 消耗咒力' 957 365 24
T '资源不足 / 被接近 → 寻找切回机会' 957 418 22 '#aebfd4'
Arrow 580 288 918 288
T 'E · 合法时切远程' 623 244 23
Arrow 918 389 580 389
T 'E · 合法时切近战' 623 407 23
Box 60 530 1380 95 '#829ebd'
T '生命：存活     行动资源：闪避 / 取消     咒力：炮击     领域能量：绝技' 88 558 28
Arrow 750 632 750 683 '#baabec'
Box 60 690 1380 213 '#baabec'
T 'R · 领域是叠加状态，不是第三个形态' 88 716 29 '#d3c3ff'
T '近战规则保留；远程成功发射后对领域参与者必中，仍消耗咒力。' 88 770 26
T '可以打断发射前蓄力；倒地保护仍有效；双方领域同时有效时相互压制必中。' 88 818 24
T '到时 / 死亡 / 重置 → 清理领域，回到正常攻防。' 88 857 23 '#aebfd4'
T '原型建议 · 不代表已实现 · 形态切换不能刷新冷却或免费取消后摇 · 详见 Docs/08' 62 952 21 '#91a5c0'
Save 'ishigori-combat-loop.png'
