#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iomanip>
#include <map>

using namespace std;

enum class TokenType {
    // Ключевые слова
    AND, BREAK, DO, ELSE, ELSEIF, END, FALSE, FOR, FUNCTION,
    GOTO, IF, IN, LOCAL, NIL, NOT, OR, REPEAT, RETURN,
    THEN, TRUE, UNTIL, WHILE,

    // Идентификаторы и литералы
    IDENTIFIER, NUMBER, STRING, LONG_STRING,

    // Операторы
    PLUS, MINUS, MUL, DIV, MOD, POW, LEN,
    BITAND, BITOR, BITNOT, BITSHL, BITSHR, IDIV,
    EQ, NEQ, LTE, GTE, LT, GT, ASSIGN,

    // Скобки и разделители
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMI, COLON, COMMA, CONCAT, DOTS, DOT,

    // Метки и другие
    LABEL, EOF_TOKEN, UNKNOWN
};

struct Token {
    TokenType type;
    string value;
    int line;
    int column;
};

class Lexer {
    string input;
    size_t pos = 0;
    int line = 1;
    int column = 1;
    Token prevToken;
    unordered_map<string, TokenType> keywords = {
        {"and", TokenType::AND}, {"break", TokenType::BREAK},
        {"do", TokenType::DO}, {"else", TokenType::ELSE},
        {"elseif", TokenType::ELSEIF}, {"end", TokenType::END},
        {"false", TokenType::FALSE}, {"for", TokenType::FOR},
        {"function", TokenType::FUNCTION}, {"goto", TokenType::GOTO},
        {"if", TokenType::IF}, {"in", TokenType::IN}, {"local", TokenType::LOCAL},
        {"nil", TokenType::NIL}, {"not", TokenType::NOT}, {"or", TokenType::OR},
        {"repeat", TokenType::REPEAT}, {"return", TokenType::RETURN},
        {"then", TokenType::THEN}, {"true", TokenType::TRUE},
        {"until", TokenType::UNTIL}, {"while", TokenType::WHILE}
    };

    char current() { return (pos < input.size()) ? input[pos] : '\0'; }
    
    char advance() { 
        char c = current();
        if (c == '\n') {
            line++;
            column = 0;
        }
        pos++;
        column++;
        return c;
    }

    void skipWhitespace() {
        while (isspace(current())) {
            advance();
        }
    }

    void skipComment() {
        if (current() == '-' && pos + 1 < input.size() && input[pos + 1] == '-') {
            advance(); advance();
            if (current() == '[') {
                advance();
                int eqCount = 0;
                while (current() == '=') {
                    eqCount++;
                    advance();
                }
                if (current() == '[') {
                    // Многострочный комментарий
                    advance();
                    string closePattern = "]";
                    for (int i = 0; i < eqCount; i++) closePattern += "=";
                    closePattern += "]";
                    while (pos + closePattern.size() <= input.size()) {
                        if (input.substr(pos, closePattern.size()) == closePattern) {
                            pos += closePattern.size();
                            column += closePattern.size();
                            return;
                        }
                        advance();
                    }
                } else {
                    // Однострочный комментарий после --[ (но не --[[)
                    while (current() != '\n' && current() != '\0') advance();
                    return;
                }
            } else {
                // Обычный однострочный комментарий
                while (current() != '\n' && current() != '\0') advance();
                return;
            }
        }
    }

    string readNumber() {
        string num;
        if (current() == '0' && (pos + 1 < input.size()) && tolower(input[pos + 1]) == 'x') {
            // Шестнадцатеричное число
            num += advance(); // '0'
            num += advance(); // 'x'
            while (isxdigit(current())) num += advance();
            if (current() == '.') {
                num += advance();
                while (isxdigit(current())) num += advance();
            }
            if (tolower(current()) == 'p') {
                num += advance();
                if (current() == '+' || current() == '-') num += advance();
                while (isdigit(current())) num += advance();
            }
        } else {
            // Десятичное число
            while (isdigit(current())) num += advance();
            if (current() == '.') {
                num += advance();
                while (isdigit(current())) num += advance();
            }
            if (tolower(current()) == 'e') {
                num += advance();
                if (current() == '+' || current() == '-') num += advance();
                while (isdigit(current())) num += advance();
            }
        }
        return num;
    }

    string readString(char delim) {
        string str;
        advance(); // Пропускаем начальную кавычку
        while (current() != delim && current() != '\0') {
            if (current() == '\\') {
                advance(); // Пропускаем '\'
                // Обработка escape-последовательностей
                switch (current()) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case 'r': str += '\r'; break;
                    case '"': str += '"'; break;
                    case '\'': str += '\''; break;
                    case '\\': str += '\\'; break;
                    default: str += '\\'; str += current(); break;
                }
                advance();
            } else {
                str += advance();
            }
        }
        advance(); // Пропускаем закрывающую кавычку
        return str;
    }

    string readLongString() {
        advance(); // '['
        int eqCount = 0;
        while (current() == '=') {
            eqCount++;
            advance();
        }
        if (current() != '[') return ""; // Некорректный формат
        
        advance(); // '['
        string str;
        string closePattern = "]";
        for (int i = 0; i < eqCount; i++) closePattern += "=";
        closePattern += "]";
        
        while (pos + closePattern.size() <= input.size()) {
            if (input.substr(pos, closePattern.size()) == closePattern) {
                pos += closePattern.size();
                column += closePattern.size();
                return str;
            }
            str += advance();
        }
        return str;
    }

    string readIdentifier() {
        string id;
        while (isalnum(current()) || current() == '_') {
            id += advance();
        }
        return id;
    }

public:
    Lexer(const string& input) : input(input) {}
    
    Token nextToken() {
    while (true) {
        skipWhitespace();
        skipComment();
        
        if (current() == '\0') break;


        char c = current();
        int startLine = line;
        int startColumn = column;

        // Длинные строки [[...]] или [=[...]=]
        if (c == '[') {
            size_t savePos = pos;
            int saveLine = line;
            int saveColumn = column;
            advance();
            if (current() == '[' || current() == '=') {
                string value = readLongString();
                if (!value.empty()) {
                    Token token{TokenType::LONG_STRING, value, startLine, startColumn};
                    prevToken = token;
                    return token;
                }
            }
            // Откат, если не получилось прочитать длинную строку
            pos = savePos;
            line = saveLine;
            column = saveColumn;
            c = current();
        }

        // Обычные строки
        if (c == '"' || c == '\'') {
            string value = readString(c);
            Token token{TokenType::STRING, value, startLine, startColumn};
            prevToken = token;
            return token;
        }

        // Числа (включая .5)
        if (isdigit(c) || (c == '.' && isdigit(input[pos + 1]))) {
            string value = readNumber();
            Token token{TokenType::NUMBER, value, startLine, startColumn};
            prevToken = token;
            return token;
        }

        // Идентификаторы и ключевые слова
        if (isalpha(c) || c == '_') {
            string id = readIdentifier();
            TokenType type = keywords.count(id) ? keywords[id] : TokenType::IDENTIFIER;
            
            // Проверка на метку ::label::
            if (current() == ':' && pos + 1 < input.size() && input[pos + 1] == ':') {
                advance(); advance();
                Token token{TokenType::LABEL, id, startLine, startColumn};
                prevToken = token;
                return token;
            }
            
            Token token{type, id, startLine, startColumn};
            prevToken = token;
            return token;
        }

        // Операторы и символы
        Token token;
        switch(c) {
            case '+': 
                advance(); 
                token = {TokenType::PLUS, "+", startLine, startColumn};
                break;
            case '-': 
                advance(); 
                token = {TokenType::MINUS, "-", startLine, startColumn};
                break;
            case '*': 
                advance(); 
                token = {TokenType::MUL, "*", startLine, startColumn};
                break;
            case '/': 
                advance(); 
                if (current() == '/') {
                    advance();
                    token = {TokenType::IDIV, "//", startLine, startColumn};
                } else {
                    token = {TokenType::DIV, "/", startLine, startColumn};
                }
                break;
            case '%': 
                advance(); 
                token = {TokenType::MOD, "%", startLine, startColumn};
                break;
            case '^': 
                advance(); 
                token = {TokenType::POW, "^", startLine, startColumn};
                break;
            case '#': 
                advance(); 
                token = {TokenType::LEN, "#", startLine, startColumn};
                break;
            case '&': 
                advance(); 
                token = {TokenType::BITAND, "&", startLine, startColumn};
                break;
            case '|': 
                advance(); 
                token = {TokenType::BITOR, "|", startLine, startColumn};
                break;
            case '~': 
                advance(); 
                if (current() == '=') {
                    advance();
                    token = {TokenType::NEQ, "~=", startLine, startColumn};
                } else {
                    token = {TokenType::BITNOT, "~", startLine, startColumn};
                }
                break;
            case '<': 
                advance(); 
                if (current() == '<') {
                    advance();
                    token = {TokenType::BITSHL, "<<", startLine, startColumn};
                } else if (current() == '=') {
                    advance();
                    token = {TokenType::LTE, "<=", startLine, startColumn};
                } else {
                    token = {TokenType::LT, "<", startLine, startColumn};
                }
                break;
            case '>': 
                advance(); 
                if (current() == '>') {
                    advance();
                    token = {TokenType::BITSHR, ">>", startLine, startColumn};
                } else if (current() == '=') {
                    advance();
                    token = {TokenType::GTE, ">=", startLine, startColumn};
                } else {
                    token = {TokenType::GT, ">", startLine, startColumn};
                }
                break;
            case '=': 
                advance(); 
                if (current() == '=') {
                    advance();
                    token = {TokenType::EQ, "==", startLine, startColumn};
                } else {
                    token = {TokenType::ASSIGN, "=", startLine, startColumn};
                }
                break;
            case '.': 
                advance(); 
                if (current() == '.') {
                    advance();
                    if (current() == '.') {
                        advance();
                        token = {TokenType::DOTS, "...", startLine, startColumn};
                    } else {
                        token = {TokenType::CONCAT, "..", startLine, startColumn};
                    }
                } else if (isdigit(current())) {
                    string num = "." + readNumber();
                    token = {TokenType::NUMBER, num, startLine, startColumn};
                } else {
                    token = {TokenType::DOT, ".", startLine, startColumn};
                }
                break;
            case '(': 
                advance(); 
                token = {TokenType::LPAREN, "(", startLine, startColumn};
                break;
            case ')': 
                advance(); 
                token = {TokenType::RPAREN, ")", startLine, startColumn};
                break;
            case '{': 
                advance(); 
                token = {TokenType::LBRACE, "{", startLine, startColumn};
                break;
            case '}': 
                advance(); 
                token = {TokenType::RBRACE, "}", startLine, startColumn};
                break;
            case '[': 
                advance(); 
                token = {TokenType::LBRACKET, "[", startLine, startColumn};
                break;
            case ']': 
                advance(); 
                token = {TokenType::RBRACKET, "]", startLine, startColumn};
                break;
            case ';': 
                advance(); 
                token = {TokenType::SEMI, ";", startLine, startColumn};
                break;
            case ':': 
                advance(); 
                if (current() == ':') {
                    advance();
                    token = {TokenType::LABEL, "::", startLine, startColumn};
                } else {
                    token = {TokenType::COLON, ":", startLine, startColumn};
                }
                break;
            case ',': 
                advance(); 
                token = {TokenType::COMMA, ",", startLine, startColumn};
                break;
            default: 
                char unknownChar = advance();
                    if (unknownChar == '\0') {
                        token = {TokenType::EOF_TOKEN, "", line, column};
                    } else if (!isspace(unknownChar)) {
                        token = {TokenType::UNKNOWN, string(1, unknownChar), startLine, startColumn};
                    } else {
                        continue;
                    }
            }
        
        prevToken = token; // Сохраняем текущий токен как предыдущий
        return token;
        }
        return {TokenType::EOF_TOKEN, "", line, column};
    }
};

string readFile(const string& filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Error: Cannot open file " << filename << endl;
        exit(1);
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

string tokenTypeToString(TokenType type) {
    static const unordered_map<TokenType, string> typeNames = {
        {TokenType::AND, "AND"}, {TokenType::BREAK, "BREAK"}, {TokenType::DO, "DO"},
        {TokenType::ELSE, "ELSE"}, {TokenType::ELSEIF, "ELSEIF"}, {TokenType::END, "END"},
        {TokenType::FALSE, "FALSE"}, {TokenType::FOR, "FOR"}, {TokenType::FUNCTION, "FUNCTION"},
        {TokenType::GOTO, "GOTO"}, {TokenType::IF, "IF"}, {TokenType::IN, "IN"},
        {TokenType::LOCAL, "LOCAL"}, {TokenType::NIL, "NIL"}, {TokenType::NOT, "NOT"},
        {TokenType::OR, "OR"}, {TokenType::REPEAT, "REPEAT"}, {TokenType::RETURN, "RETURN"},
        {TokenType::THEN, "THEN"}, {TokenType::TRUE, "TRUE"}, {TokenType::UNTIL, "UNTIL"},
        {TokenType::WHILE, "WHILE"}, {TokenType::IDENTIFIER, "IDENTIFIER"},
        {TokenType::NUMBER, "NUMBER"}, {TokenType::STRING, "STRING"},
        {TokenType::LONG_STRING, "LONG_STRING"}, {TokenType::PLUS, "PLUS"},
        {TokenType::MINUS, "MINUS"}, {TokenType::MUL, "MUL"}, {TokenType::DIV, "DIV"},
        {TokenType::MOD, "MOD"}, {TokenType::POW, "POW"}, {TokenType::LEN, "LEN"},
        {TokenType::BITAND, "BITAND"}, {TokenType::BITOR, "BITOR"}, {TokenType::BITNOT, "BITNOT"},
        {TokenType::BITSHL, "BITSHL"}, {TokenType::BITSHR, "BITSHR"}, {TokenType::IDIV, "IDIV"},
        {TokenType::EQ, "EQ"}, {TokenType::NEQ, "NEQ"}, {TokenType::LTE, "LTE"},
        {TokenType::GTE, "GTE"}, {TokenType::LT, "LT"}, {TokenType::GT, "GT"},
        {TokenType::ASSIGN, "ASSIGN"}, {TokenType::LPAREN, "LPAREN"}, {TokenType::RPAREN, "RPAREN"},
        {TokenType::LBRACE, "LBRACE"}, {TokenType::RBRACE, "RBRACE"}, {TokenType::LBRACKET, "LBRACKET"},
        {TokenType::RBRACKET, "RBRACKET"}, {TokenType::SEMI, "SEMI"}, {TokenType::COLON, "COLON"},
        {TokenType::COMMA, "COMMA"}, {TokenType::CONCAT, "CONCAT"}, {TokenType::DOTS, "DOTS"},  {TokenType::DOT, "DOT"},
        {TokenType::LABEL, "LABEL"}, {TokenType::EOF_TOKEN, "EOF_TOKEN"}, {TokenType::UNKNOWN, "UNKNOWN"}
    };
    return typeNames.at(type);
}

int main() {
    string code = readFile("init.lua");
    Lexer lexer(code);
    string str = "";
    vector<Token> unknownTokens;
    
    while (true) {
        Token token = lexer.nextToken();
        if (token.type == TokenType::EOF_TOKEN) break;
        if (token.type == TokenType::UNKNOWN) {
            unknownTokens.push_back(token);
        }
        str += tokenTypeToString(token.type) + " ";
        cout << "Line " << token.line << ":" << token.column
             << " \tType: " << tokenTypeToString(token.type)
             << " \tValue: " << token.value << endl;
    }

    stringstream ss(str);
    string word;
    map<string, int> word_count;

    // Разбиваем строку на слова
    while (ss >> word) {
        // Преобразуем слово в нижний регистр
        for (char &c : word) {
            c = tolower(c);
        }

        // Удаляем знаки пунктуации
        string punctuation = "!?,.-";
        for (char c : punctuation) {
            size_t pos = word.find(c);
            while (pos != string::npos) {
                word.erase(pos, 1);
                pos = word.find(c, pos);
            }
        }
        
        if (!word.empty()) {
            word_count[word]++;
        }
    }

    // Вычисляем ОБЩЕЕ количество слов (сумму всех счетчиков)
    int total_count = 0;
    for (const auto &pair : word_count) {
        total_count += pair.second;
    }

    // Выводим результат с правильным расчетом частоты
    cout << fixed << setprecision(2); // Фиксируем вывод до 2 знаков после запятой
    for (const auto &pair : word_count) {
        // Правильное вычисление частоты:
        double frequency = (static_cast<double>(pair.second) / total_count) * 100;
        cout << pair.first << ": " << pair.second << " | " << frequency << "%" << endl;
    }

    if (unknownTokens.empty()) {
        cout << "No unknown tokens found." << endl;
    } else {
        cout << "\nUnknown tokens found:" << endl;
        for (const Token& t : unknownTokens) {
            cout << "Line " << t.line << ":" << t.column
                 << " \tValue: '" << t.value << "'" << endl;
        }
    }
    
    return 0;
}
