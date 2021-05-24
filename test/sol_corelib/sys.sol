: main
  "ds size" [ sys:info:ds_size ] CHECK
  "rs size" [ sys:info:rs_size ] CHECK

  "cell size" [
    sys:info:cell_size 1 cells =
  ] CHECK

  "int min and max" [
    sys:info:max_int sys:info:min_int + -1 =
  ] CHECK
;