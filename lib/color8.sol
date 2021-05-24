( ===== 8bit RBG/HSV Color ===== )

: rgb8
  : merge ( r g b -- rgb8 )
    >r 8 << >r 16 << r> bit-or r> bit-or ;
  : split ( rgb8 -- r g b )
    dup dup
    [ 0xFF0000 bit-and 16 >> ]
    [ 0x00FF00 bit-and 8  >> ]
    [ 0x0000FF bit-and ] tri*
  ;
  : to_hsv ( r g b -- h s v )
    val: r  val: g  val: b
    val: h  val: s  val: v
    val: rgbmax  val: rgbmin  val: drgb
    b! g! r!

    r g b max max rgbmax!
    r g b min min rgbmin!
    rgbmax rgbmin - drgb!

    ( compute value )
    rgbmax v!
    v 0 = IF 0 0 0 RET END

   ( compute saturation )
    drgb 255 * v / s!
    s 0 = IF 0 0 0 RET END

    ( compute hue and return )
    rgbmax r = IF g b - 43 * drgb /        s v RET END
    rgbmax g = IF b r - 43 * drgb /  85  + s v RET END
         ( b =  ) r g - 43 * drgb /  171 + s v
  ;
  : from_hsv ( h s v -- r g b )
    val: r  val: g  val: b
    val: h  val: s  val: v
    val: p  val: q  val: t
    val: region  val: remainder
    : tmp 255 swap - v * 8 >> ;
    v! s! h!

    ( grayscale )
    s 0 = IF v v v RET END

    h 43 / region!
    h region 43 * - 6 * remainder!

    ( tmp )
    s tmp p!
    s remainder * 8 >> tmp q!
    255 remainder - s * 8 >> tmp t!

    region 0 = IF v t p RET END
    region 1 = IF q v p RET END
    region 2 = IF p v t RET END
    region 3 = IF p q v RET END
    region 4 = IF t p v RET END
    v p q
  ;
;