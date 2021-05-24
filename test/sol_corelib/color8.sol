include: "color8.sol"

: main
  : show3 cr [ ? ] tria ;
  "rbg8 merge/split 1" [ 0x0A 0x0B 0x0C rgb8:merge 0x0A0B0C = ] CHECK
  "rgb8 merge/split 2" [ 0x0A0B0C dup rgb8:split rgb8:merge = ] CHECK

  "rgb8 to_hsv/from_hsv" [
    ( Ignore minor difference and check stack balancing )
    0x102030 rgb8:split rgb8:to_hsv rgb8:from_hsv rgb8:merge
  ] CHECK
;