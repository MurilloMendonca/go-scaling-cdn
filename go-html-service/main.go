package main

import (
	"fmt"
	"io"
	"math/rand"
	"net"
	"net/http"
	"os"
	"strings"
	"time"
	"unsafe"

	"github.com/gorilla/mux"
	"github.com/redis/go-redis/v9"
)

// #cgo LDFLAGS: -L../.
// #include "../cpp-processing-service/task.h"
import "C"

var letters = []rune("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")

func randSeq(n int) string {
    b := make([]rune, n)
    for i := range b {
        b[i] = letters[rand.Intn(len(letters))]
    }
    return string(b)
}
func main() {
    client := redis.NewClient(&redis.Options{
        Addr:	  "localhost:6379",
        Password: "", // no password set
        DB:		  0,  // use default DB
    })
    r := mux.NewRouter()
    fs := http.FileServer(http.Dir("uploads"))
    r.PathPrefix("/uploads/").Handler(http.StripPrefix("/uploads/", fs))

    r.HandleFunc("/getCachedImage", func(w http.ResponseWriter, r *http.Request) {
        fileName := r.URL.Query().Get("fileName")
        val, err := client.Get(r.Context(),fileName).Result()
        if err != nil {
            http.Error(w, err.Error(), http.StatusInternalServerError)
            return
        }
        w.Header().Set("Content-Type", "image/png")
        w.Write([]byte(val))
    })



    r.HandleFunc("/form", func(w http.ResponseWriter, r *http.Request) {
        if r.Method != "POST" {
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
            return
        }

        if err := r.ParseMultipartForm(10 << 20); err != nil {
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

        fileName := "uploads/" + name + "-" + randSeq(8) + "-" + fmt.Sprintf("%d", time.Now().Unix()) + "." +strings.Split(handler.Filename, ".")[1]
        f, err := os.OpenFile(fileName, os.O_WRONLY|os.O_CREATE, 0666)
        if err != nil {
            http.Error(w, err.Error(), http.StatusInternalServerError)
            return
        }
        defer f.Close()
        io.Copy(f, file)

        scaledFileName := strings.Split(fileName, ".")[0] + "-64x64." + strings.Split(fileName, ".")[1]
        quantizedFileName := strings.Split(fileName, ".")[0] + "-quantized." + strings.Split(fileName, ".")[1]

        task := C.makeTaskPackage(C.CString(fileName), C.CString(scaledFileName), C.CString(quantizedFileName),64, 64)

        // call the scalling service on port 8989 using sockets
        conn, err := net.Dial("tcp", "localhost:8989")
        if err != nil {
            http.Error(w, err.Error(), http.StatusInternalServerError)
            return
        }
    

        defer conn.Close()

        encodedTask := C.encodeTask(task)
        goEncodeTask := C.GoString(encodedTask)

        conn.Write([]byte(goEncodeTask))

        C.destroyTaskPackage(task)
        C.free(unsafe.Pointer(encodedTask))
        buf := make([]byte, 2048)
        conn.Read(buf)

        taskResponse := C.decodeTaskResult(C.CString(string(buf)))
        fmt.Println("Task sucess: ", taskResponse.SUCCESS)
        conn.Close()
        C.destroyTaskResult(taskResponse)

        scaledFile, err := os.Open(scaledFileName)
        if err != nil {
            http.Error(w, err.Error(), http.StatusInternalServerError)
            return
        }
        defer scaledFile.Close()

        scaledFileBytes := make([]byte, 64*64*3)
        scaledFile.Read(scaledFileBytes)
        client.Set(r.Context(), scaledFileName, scaledFileBytes, 0)
        
        w.Header().Set("Content-Type", "application/json")
        w.Write([]byte(fmt.Sprintf(`{"original": "%s", "scaled": "%s", "quantized": "%s"}`, fileName, scaledFileName, quantizedFileName)))

    })

    http.ListenAndServe(":6969", r)
}
