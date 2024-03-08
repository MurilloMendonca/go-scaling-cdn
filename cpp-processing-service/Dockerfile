FROM alpine
RUN apk add --no-cache libc6-compat
WORKDIR /app
COPY ./server /app/
EXPOSE 8989
CMD ["./server"]
