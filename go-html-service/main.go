package main

import (
	"fmt"
	"github.com/gorilla/mux"
	"github.com/redis/go-redis/v9"
	"io"
	"math/rand"
	"net"
	"net/http"
	"os"
	"strings"
	"time"
)

const (
	uploadPath            = "uploads/"
	imageProcessingHost   = "127.0.0.1" // Default value, can be overridden by environment variables
	imageProcessingPort   = "8989"      // Default value, can be overridden by environment variables
	serverPort            = ":6969"
	imageScaleDimensionsX  = 64
    imageScaleDimensionsY  = 64
    quantizeColors        = 8
	redisAddr             = "localhost:6379"
	filePermission        = 0666
	multipartFormMaxBytes = 10 << 20 // 10 MB
)

var letters = []rune("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")

func main() {
    redisClientAddr := redisAddr
    // Check if there is a environment variable for the redis server
    if os.Getenv("REDIS_SERVICE_HOST") != "" {
        redisClientAddr = os.Getenv("REDIS_SERVICE_HOST") + ":" + os.Getenv("REDIS_SERVICE_PORT")
    }
	client := redis.NewClient(&redis.Options{
		Addr:     redisClientAddr,
		Password: "", // no password set
		DB:       0,  // use default DB
	})

	r := mux.NewRouter()
	initializeRoutes(r, client)
	http.ListenAndServe(serverPort, r)
}

func initializeRoutes(r *mux.Router, client *redis.Client) {
	fs := http.FileServer(http.Dir(uploadPath))
	r.PathPrefix("/" + uploadPath).Handler(http.StripPrefix("/"+uploadPath, fs))

	r.HandleFunc("/getCachedImage", getCachedImageHandler(client))
	r.HandleFunc("/form", formHandler(client))
}

func randSeq(n int) string {
	b := make([]rune, n)
	for i := range b {
		b[i] = letters[rand.Intn(len(letters))]
	}
	return string(b)
}

func getCachedImageHandler(client *redis.Client) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		fileName := r.URL.Query().Get("fileName")
		val, err := client.Get(r.Context(), fileName).Result()
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", "image/png")
		w.Write([]byte(val))
	}
}

func formHandler(client *redis.Client) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != "POST" {
			displayForm(w)
			return
		}

		handleFileUpload(w, r, client)
	}
}

func displayForm(w http.ResponseWriter) {
	w.Write([]byte(`
        <html>
            <body>
                <form method="post" enctype="multipart/form-data">
                    <input type="text" name="name" />
                    <input type="file" name="file" accept="image/png" />
                    <input type="submit" />
                </form>
            </body>
        </html>
    `))
}

func handleFileUpload(w http.ResponseWriter, r *http.Request, client *redis.Client) {
	if err := r.ParseMultipartForm(multipartFormMaxBytes); err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	name := r.FormValue("name")

	file, handler, err := r.FormFile("file")
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	defer file.Close()

    //check if the file is a png
    if !strings.Contains(handler.Filename, ".png") {
        http.Error(w, "Only PNG files are allowed", http.StatusBadRequest)
        return
    }

	fileName := createFileName(name, handler.Filename)
    f, err := os.OpenFile(fileName, os.O_WRONLY|os.O_CREATE, filePermission)
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }
    defer f.Close()
    io.Copy(f, file)
    file.Close()

	processImage(w, r, fileName, client)
}

func createFileName(name, originalFileName string) string {
	timeSuffix := fmt.Sprintf("%d", time.Now().Unix())
	extension := strings.Split(originalFileName, ".")[1]
	return fmt.Sprintf("%s%s-%s-%s.%s", uploadPath, name, randSeq(8), timeSuffix, extension)
}

func processImage(w http.ResponseWriter, r *http.Request, fileName string, client *redis.Client) {
    processingHost := imageProcessingHost
    processingPort := imageProcessingPort
    // Check if there is a environment variable for the image processing service
    if os.Getenv("CPP_SERVICE_HOST") != "" {
        processingHost = os.Getenv("CPP_SERVICE_HOST")
    }
    if os.Getenv("CPP_SERVICE_PORT") != "" {
        processingPort = os.Getenv("CPP_SERVICE_PORT")
    }
    // Connect to the image processing service
    conn, err := net.Dial("tcp", processingHost+":"+processingPort)
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }
    defer conn.Close()

    scaledFileName := strings.Split(fileName, ".")[0] + "-scaled.png"
    quantizedFileName := strings.Split(fileName, ".")[0] + "-quantized.png"
    
    scaleTask := fmt.Sprintf("s:%s:%s:%d:%d:", fileName, scaledFileName, imageScaleDimensionsX, imageScaleDimensionsY)
    quantizeTask := fmt.Sprintf("q:%s:%s:%d:", scaledFileName, quantizedFileName, quantizeColors)

    buf := make([]byte, 1024)
    conn.Write([]byte(scaleTask))
    conn.Read(buf)
    taskSuccess := strings.Contains(string(buf), "OK")

    conn.Write([]byte(quantizeTask))
    conn.Read(buf)
    taskSuccess = taskSuccess && strings.Contains(string(buf), "OK")

    if !taskSuccess {
        http.Error(w, "Image processing failed", http.StatusInternalServerError)
        return
    }

    addImageToCache(w, r, quantizedFileName, client)

    // Send back a JSON response with the file names
    w.Header().Set("Content-Type", "application/json")
    w.Write([]byte(fmt.Sprintf(`{"original": "%s", "scaled": "%s", "quantized": "%s"}`, fileName, scaledFileName, quantizedFileName)))


}

func addImageToCache(w http.ResponseWriter, r *http.Request, fileName string, client *redis.Client) {
    f, err := os.Open(fileName)
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }
    defer f.Close()

    buf := make([]byte, 2048)
    f.Read(buf)

    err = client.Set(r.Context(), fileName, buf, 0).Err()
    if err != nil {
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }
}

