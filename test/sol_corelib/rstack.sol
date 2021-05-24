: test/rpick
  1 >r 2 >r 3 >r  # rstack: 1 2 3

  0 rpick 3 = "rpick 1" ASSERT
  1 rpick 2 = "rpick 2" ASSERT
  2 rpick 1 = "rpick 3" ASSERT

  i 3 = "i" ASSERT
  j 2 = "j" ASSERT

  ( clean up )
  r> drop r> drop r> drop
;

: main "all" [ test/rpick ok ] CHECK ;