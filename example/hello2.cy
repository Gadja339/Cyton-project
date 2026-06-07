
type Meters = int64

namespace Math:
    fun square(int64 x) -> int64:
        return x * x;

    fun abs(int64 x) -> int64:
        if x < 0:
            return -x
        else:
            return x;
    ;
;

struct Point:
    int64 x
    int64 y
;

let a = 10
let b = 20
var result = a + b * 3 - 5 / 2 % 2

let ok = true and not false
let cmp1 = a == b
let cmp2 = a != b
let cmp3 = a <= b
let cmp4 = a >= b
let cmp5 = a < b
let cmp6 = a > b

let text = "Hello, Cyton!"
let arr = [1, 2, 3, 4, 5]

let value = Math::square(result)
let p = Point { x: 10, y: 20 }
let px = p.x

while result > 0:
    result = result - 1

    if result == 5:
        continue
    ;

    if result == 2:
        break
    ;
;

print(text)
print(value)
print(px)