FROM alpine:latest
RUN apk add --no-cache g++ libstdc++
WORKDIR /app
COPY server.cpp www/* .
RUN g++ -O3 -std=c++17 -pthread server.cpp -o server && mkdir www && mv script.js www/script.js && mv style.css www/style.css && mv index.md www/index.md
EXPOSE 8080 8081
CMD ["./server"]
