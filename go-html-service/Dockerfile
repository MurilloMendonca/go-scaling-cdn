FROM alpine
RUN apk add --no-cache libc6-compat
WORKDIR /app
COPY ./main /app/
EXPOSE 6969
CMD ["./main"]
