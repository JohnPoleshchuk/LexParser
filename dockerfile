FROM gcc:latest

WORKDIR /app

COPY main.cpp .
COPY test.lua .
COPY init.lua .


RUN g++ -std=c++11 -o lexer main.cpp

ENTRYPOINT ["./lexer"]
CMD ["test.lua"]