#Напишите функцию void compressSequence(int arr[], int length), которая заменяет подряд идущие повторяющиеся числа на пару (число, количество) и выводит результат.

#Пример:
#Вход: {1, 1, 1, 2, 3, 3, 4, 4, 4, 4}
#Выход: (1,3) (2,1) (3,2) (4,4)

#Условия: без указателей, использовать только индексы. Обрабатывать пустой массив и массив из одного элемента.






fun printPars(int64 num, int64 count) -> unit:
    print(num)
    print(count);

fun compressSequence(array arr, int64 length) -> unit:
    if length == 0:
        print("Пусто")
    else:
        var i = 1
        var that = arr[0]
        var count = 1

        while i < length:
            if arr[i] == that:
                count = count + 1
            else:
                printPars(that, count)

                that = arr[i]
                count = 1;

            i = i + 1;

        printPars(that, count);
;

fun main() -> int64:
    var arr = [1, 1, 1, 2, 3, 3, 4, 4, 4, 4]

    compressSequence(arr, 10)

    return 0
;