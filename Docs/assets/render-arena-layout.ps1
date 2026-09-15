# 本项目白盒布局示意，不是《异人之下》官方地图。
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::new(1440, 1000)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
function Brush([string]$hex) { [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml($hex)) }
function Pen([string]$hex, [float]$width = 2) { [System.Drawing.Pen]::new([System.Drawing.ColorTranslator]::FromHtml($hex), $width) }
function Label([string]$value, [float]$x, [float]$y, [float]$size = 22, [string]$color = '#dce6f2') {
    $font = [System.Drawing.Font]::new('Microsoft YaHei', $size, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $brush = Brush $color
    $graphics.DrawString($value, $font, $brush, $x, $y)
    $font.Dispose(); $brush.Dispose()
}
$graphics.Clear([System.Drawing.ColorTranslator]::FromHtml('#111b2a'))
Label '石流龙 Demo · 擂台白盒方案' 56 38 38
Label '参考《异人之下》的圆形空间组织 · 本项目设计，非官方地图或测绘数据' 58 96 21 '#a9b9cd'
# 圆心 (450, 520)，20 px/m；场地半径 240 px。
$graphics.FillEllipse((Brush '#1b2b3e'), 150, 220, 600, 600)
$graphics.DrawEllipse((Pen '#52647c' 2), 150, 220, 600, 600)
$graphics.FillEllipse((Brush '#473c35'), 210, 280, 480, 480)
$graphics.FillEllipse((Brush '#21394b'), 250, 320, 400, 400)
$graphics.DrawEllipse((Pen '#e1b481' 4), 210, 280, 480, 480)
$dash = Pen '#63859c' 2
$dash.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
$graphics.DrawEllipse($dash, 250, 320, 400, 400)
$graphics.DrawEllipse($dash, 390, 460, 120, 120)
$graphics.DrawLine($dash, 180, 520, 720, 520)
$graphics.DrawLine($dash, 450, 250, 450, 790)
Label '+X' 717 526 19 '#a9b9cd'
Label '+Y' 464 228 19 '#a9b9cd'
Label '圆心' 421 532 18 '#a9b9cd'
$graphics.FillEllipse((Brush '#5ebcec'), 335, 505, 30, 30)
$graphics.FillEllipse((Brush '#f49e87'), 535, 505, 30, 30)
$arrow = Pen '#5ebcec' 3
$arrow.EndCap = [System.Drawing.Drawing2D.LineCap]::ArrowAnchor
$graphics.DrawLine($arrow, 350, 520, 386, 520)
$arrow.Color = [System.Drawing.ColorTranslator]::FromHtml('#f49e87')
$graphics.DrawLine($arrow, 550, 520, 514, 520)
Label 'P1 石流龙' 292 566 22 '#7cd0ff'
Label 'P2 石流龙' 514 566 22 '#ffb5a1'
Label '(-5, 0) m' 296 602 18 '#a9b9cd'
Label '(5, 0) m' 518 602 18 '#a9b9cd'
$graphics.DrawLine((Pen '#b6c9dd'), 350, 415, 550, 415)
$graphics.DrawLine((Pen '#b6c9dd'), 350, 406, 350, 424)
$graphics.DrawLine((Pen '#b6c9dd'), 550, 406, 550, 424)
Label '开局间距 10 m' 372 374 21
Label '中央近战区 r ≈ 3 m' 354 659 20 '#91b7cf'
$graphics.DrawRectangle((Pen '#89a9ce' 2), 283, 477, 24, 18)
Label '镜头示意' 198 439 18 '#a9b9cd'
$graphics.DrawLine((Pen '#748da9'), 210, 855, 690, 855)
$graphics.DrawLine((Pen '#748da9'), 210, 843, 210, 867)
$graphics.DrawLine((Pen '#748da9'), 690, 843, 690, 867)
Label '可行走直径 24 m / 2400 cm' 279 872 23
Label '01  开阔、平坦的交战区' 832 232 27 '#7cd0ff'
Label '无柱体、无高差；支持近战与炮击。' 832 277 22
Label '02  清晰的实体碰撞边界' 832 354 27 '#e1b481'
Label '最外侧 2 m 为边缘压力测试带。' 832 399 22
Label '无墙弹、无跌落判负。' 832 435 22
Label '03  外围视觉缓冲' 832 512 27 '#a5bada'
Label '场外约 3 m，可布置背景与装饰。' 832 557 22
Label '镜头碰撞单独调试，不固定机位。' 832 593 22
Label '04  可复现的训练起点' 832 670 27 '#ffb5a1'
Label '双方朝向彼此；木桩与 AI 共用规则。' 832 715 22
Label '虚线仅为测试标记，不是障碍物。' 832 751 22
Label '尺寸均为待实测的起步参数。' 832 824 22 '#a9b9cd'
Label '设计记录 2026-09-15  /  详见 Docs/07_擂台与视觉参考.md' 58 953 19 '#8295ae'
$bitmap.Save((Join-Path $PSScriptRoot 'arena-layout.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$dash.Dispose(); $arrow.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
