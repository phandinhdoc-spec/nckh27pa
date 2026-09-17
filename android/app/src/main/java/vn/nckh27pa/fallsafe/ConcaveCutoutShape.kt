package vn.nckh27pa.fallsafe

import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import kotlin.math.atan2
import kotlin.math.sqrt

enum class CutoutCorner {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
}

/**
 * Reusable Concave Shape for the 4 perimeter cards wrapping around the center SOS button.
 *
 * Each card has 3 standard rounded outer corners (outerCornerRadius) and exactly 1 concave
 * inner cutout corner facing the center circular SOS button.
 *
 * The cutout arc is mathematically concentric with the center SOS button, ensuring a uniform
 * clearance gap (typically 8-16dp) around the SOS button.
 */
class ConcaveCutoutShape(
    val cutoutCorner: CutoutCorner,
    val cutoutRadius: Dp,
    val outerCornerRadius: Dp = 16.dp,
    val gapX: Dp = 6.dp,
    val gapY: Dp = 6.dp
) : Shape {

    override fun createOutline(
        size: Size,
        layoutDirection: LayoutDirection,
        density: Density
    ): Outline {
        val w = size.width
        val h = size.height
        if (w <= 0f || h <= 0f) return Outline.Generic(Path())

        val R = with(density) { cutoutRadius.toPx() }
        val r = with(density) { outerCornerRadius.toPx().coerceAtMost(minOf(w, h) / 4f) }
        val gx = with(density) { gapX.toPx() }
        val gy = with(density) { gapY.toPx() }

        val path = Path().apply {
            when (cutoutCorner) {
                CutoutCorner.BOTTOM_RIGHT -> {
                    // Top-Left card. Cutout at bottom-right (w, h).
                    // Center of SOS circle: (w + gx, h + gy)
                    val cx = w + gx
                    val cy = h + gy
                    val dy = if (R > gx) sqrt(R * R - gx * gx) else 0f
                    val dx = if (R > gy) sqrt(R * R - gy * gy) else 0f
                    val yCut = (cy - dy).coerceIn(r, h)
                    val xCut = (cx - dx).coerceIn(r, w)

                    val th1 = Math.toDegrees(atan2((yCut - cy).toDouble(), (w - cx).toDouble())).toFloat()
                    val th2 = Math.toDegrees(atan2((h - cy).toDouble(), (xCut - cx).toDouble())).toFloat()
                    var sweep = th2 - th1
                    if (sweep > 0) sweep -= 360f

                    moveTo(r, 0f)
                    lineTo(w - r, 0f)
                    quadraticTo(w, 0f, w, r)
                    lineTo(w, yCut)
                    arcTo(
                        rect = Rect(cx - R, cy - R, cx + R, cy + R),
                        startAngleDegrees = th1,
                        sweepAngleDegrees = sweep,
                        forceMoveTo = false
                    )
                    lineTo(r, h)
                    quadraticTo(0f, h, 0f, h - r)
                    lineTo(0f, r)
                    quadraticTo(0f, 0f, r, 0f)
                    close()
                }

                CutoutCorner.BOTTOM_LEFT -> {
                    // Top-Right card. Cutout at bottom-left (0, h).
                    // Center of SOS circle: (-gx, h + gy)
                    val cx = -gx
                    val cy = h + gy
                    val dy = if (R > gx) sqrt(R * R - gx * gx) else 0f
                    val dx = if (R > gy) sqrt(R * R - gy * gy) else 0f
                    val xCut = (cx + dx).coerceIn(0f, w - r)
                    val yCut = (cy - dy).coerceIn(r, h)

                    val th1 = Math.toDegrees(atan2((h - cy).toDouble(), (xCut - cx).toDouble())).toFloat()
                    val th2 = Math.toDegrees(atan2((yCut - cy).toDouble(), (0f - cx).toDouble())).toFloat()
                    var sweep = th2 - th1
                    if (sweep > 0) sweep -= 360f

                    moveTo(r, 0f)
                    lineTo(w - r, 0f)
                    quadraticTo(w, 0f, w, r)
                    lineTo(w, h - r)
                    quadraticTo(w, h, w - r, h)
                    lineTo(xCut, h)
                    arcTo(
                        rect = Rect(cx - R, cy - R, cx + R, cy + R),
                        startAngleDegrees = th1,
                        sweepAngleDegrees = sweep,
                        forceMoveTo = false
                    )
                    lineTo(0f, r)
                    quadraticTo(0f, 0f, r, 0f)
                    close()
                }

                CutoutCorner.TOP_RIGHT -> {
                    // Bottom-Left card. Cutout at top-right (w, 0).
                    // Center of SOS circle: (w + gx, -gy)
                    val cx = w + gx
                    val cy = -gy
                    val dy = if (R > gx) sqrt(R * R - gx * gx) else 0f
                    val dx = if (R > gy) sqrt(R * R - gy * gy) else 0f
                    val xCut = (cx - dx).coerceIn(r, w)
                    val yCut = (cy + dy).coerceIn(0f, h - r)

                    val th1 = Math.toDegrees(atan2((0f - cy).toDouble(), (xCut - cx).toDouble())).toFloat()
                    val th2 = Math.toDegrees(atan2((yCut - cy).toDouble(), (w - cx).toDouble())).toFloat()
                    var sweep = th2 - th1
                    if (sweep > 0) sweep -= 360f

                    moveTo(r, 0f)
                    lineTo(xCut, 0f)
                    arcTo(
                        rect = Rect(cx - R, cy - R, cx + R, cy + R),
                        startAngleDegrees = th1,
                        sweepAngleDegrees = sweep,
                        forceMoveTo = false
                    )
                    lineTo(w, h - r)
                    quadraticTo(w, h, w - r, h)
                    lineTo(r, h)
                    quadraticTo(0f, h, 0f, h - r)
                    lineTo(0f, r)
                    quadraticTo(0f, 0f, r, 0f)
                    close()
                }

                CutoutCorner.TOP_LEFT -> {
                    // Bottom-Right card. Cutout at top-left (0, 0).
                    // Center of SOS circle: (-gx, -gy)
                    val cx = -gx
                    val cy = -gy
                    val dy = if (R > gx) sqrt(R * R - gx * gx) else 0f
                    val dx = if (R > gy) sqrt(R * R - gy * gy) else 0f
                    val yCut = (cy + dy).coerceIn(0f, h - r)
                    val xCut = (cx + dx).coerceIn(0f, w - r)

                    val th1 = Math.toDegrees(atan2((yCut - cy).toDouble(), (0f - cx).toDouble())).toFloat()
                    val th2 = Math.toDegrees(atan2((0f - cy).toDouble(), (xCut - cx).toDouble())).toFloat()
                    var sweep = th2 - th1
                    if (sweep > 0) sweep -= 360f

                    moveTo(xCut, 0f)
                    lineTo(w - r, 0f)
                    quadraticTo(w, 0f, w, r)
                    lineTo(w, h - r)
                    quadraticTo(w, h, w - r, h)
                    lineTo(r, h)
                    quadraticTo(0f, h, 0f, h - r)
                    lineTo(0f, yCut)
                    arcTo(
                        rect = Rect(cx - R, cy - R, cx + R, cy + R),
                        startAngleDegrees = th1,
                        sweepAngleDegrees = sweep,
                        forceMoveTo = false
                    )
                    close()
                }
            }
        }
        return Outline.Generic(path)
    }
}
