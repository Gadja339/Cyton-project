fun checkLine(string a, string b, string c) -> int64:
    if a == "X" and b == "X" and c == "X":
        return 1;

    if a == "O" and b == "O" and c == "O":
        return 2;

    return 0;


# 0 1 2
# 3 4 5 
# 6 7 8

fun checkWinner(array board) -> int64:
    var int64 result = 0

    result = checkLine(board[0], board[1], board[2])
    if result != 0:
        return result;

    result = checkLine(board[3], board[4], board[5])
    if result != 0:
        return result;

    result = checkLine(board[6], board[7], board[8])
    if result != 0:
        return result;

    result = checkLine(board[0], board[3], board[6])
    if result != 0:
        return result;

    result = checkLine(board[1], board[4], board[7])
    if result != 0:
        return result;

    result = checkLine(board[2], board[5], board[8])
    if result != 0:
        return result;

    result = checkLine(board[0], board[4], board[8])
    if result != 0:
        return result;

    result = checkLine(board[2], board[4], board[6])
    if result != 0:
        return result;

    return 0;

# X 0 0 
# 0 . 0
# . . X


fun main() -> int64:
    var board = ["X", "O", "O", "X", ".", "X", ".", ".", "X"]

    print(checkWinner(board))

    return 0;