#Напишите функцию void printNumberTriangle(int n), которая выводит треугольник:

#Для n = 4:

#1
#2 3
#4 5 6
#7 8 9 10
#Условия: нельзя использовать вложенный if внутри цикла для определения границ строк. Использовать один счётчик возрастающих чисел и вложенный for.


fun printNumberTriangle(int64 n) -> unit:
    var now = 1
    var row = 1

    while row <= n:
        var col = 1

        while col <= row:
            print(now)

            now = now + 1
            col = col + 1;

        print("Конец строки")
        row = row + 1;
;

fun main() -> int64:
    printNumberTriangle(5)

    return 0;

#1
#2 3
# 4 5 6 
# 7 8 9 10