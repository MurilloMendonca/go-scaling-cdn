package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"html/template"
	"io"
	"mime/multipart"
	"net/http"
	"strconv"
	"time"

	"github.com/labstack/echo/v4"
	"github.com/labstack/echo/v4/middleware"
)

type Template struct {
	tmpl *template.Template
}

func newTemplate() *Template {
	return &Template{
		tmpl: template.Must(template.ParseGlob("views/*.html")),
	}
}

func (t *Template) Render(w io.Writer, name string, data interface{}, c echo.Context) error {
	return t.tmpl.ExecuteTemplate(w, name, data)
}

type Contact struct {
	Name  string
	Email string
	Id    int
    ProfilePicture string
    ProfilePictureIcon string
}

type Data struct {
	Contacts []Contact
}

func NewData() *Data {
	return &Data{
		Contacts: []Contact{},
	}
}

type FormData struct {
	Errors map[string]string
	Values map[string]string
}

func NewFormData() FormData {
	return FormData{
		Errors: map[string]string{},
		Values: map[string]string{},
	}
}

type PageData struct {
	Data Data
	Form FormData
}

func NewContact(id int, name, email, profilePic, profileIcon string) Contact {
	return Contact{
		Id:    id,
		Name:  name,
		Email: email,
        ProfilePicture: profilePic,
        ProfilePictureIcon: profileIcon,
	}
}

func NewPageData(data Data, form FormData) PageData {
	return PageData{
		Data: data,
		Form: form,
	}
}

func contactExists(contacts []Contact, email string) bool {
	for _, c := range contacts {
		if c.Email == email {
			return true
		}
	}
	return false
}

func main() {

	e := echo.New()

	data := NewData()
	id := 3

	e.Renderer = newTemplate()
	e.Use(middleware.Logger())
    e.Static("/images", "images")
    e.Static("/css", "css")

	e.GET("/", func(c echo.Context) error {
		return c.Render(200, "index.html", NewPageData(*data, NewFormData()))
	})

	e.POST("/contacts", func(c echo.Context) error {
		name := c.FormValue("name")
		email := c.FormValue("email")

		if contactExists(data.Contacts, email) {
			formData := FormData{
				Errors: map[string]string{
					"email": "Email already exists",
				},
				Values: map[string]string{
					"name":  name,
					"email": email,
				},
			}

			return c.Render(422, "contact-form", formData)
		}

        profilePicture,_ := c.FormFile("file")
        if profilePicture == nil {
            formData := FormData{
                Errors: map[string]string{
                    "file": "Please upload a profile picture",
                },
                Values: map[string]string{
                    "name":  name,
                    "email": email,
                },
            }

            return c.Render(422, "contact-form", formData)
        }
        profilePicPath, profileIconPath := uploadImage(profilePicture)
		contact := NewContact(id, name, email, profilePicPath, profileIconPath)
		id++
		data.Contacts = append(data.Contacts, contact)

		formData := NewFormData()
		err := c.Render(200, "contact-form", formData)

		if err != nil {
			return err
		}

		return c.Render(200, "oob-contact", contact)
	})

    e.GET("/contacts/:id/modal", func(c echo.Context) error {
        idStr := c.Param("id")
        id, err := strconv.Atoi(idStr)
        if err != nil {
            return c.String(400, "Id must be an integer")
        }

        for _, contact := range data.Contacts {
            if contact.Id == id {
                return c.Render(200, "modal", contact)
            }
        }

        return c.String(400, "Contact not found")


    })
	e.DELETE("/contacts/:id", func(c echo.Context) error {
		idStr := c.Param("id")
        id, err := strconv.Atoi(idStr)

        if err != nil {
            return c.String(400, "Id must be an integer")
        }

        deleted := false
		for i, contact := range data.Contacts {
			if contact.Id == id {
				data.Contacts = append(data.Contacts[:i], data.Contacts[i+1:]...)
                deleted = true
                break
			}
		}

        if !deleted {
            return c.String(400, "Contact not found")
        }

        time.Sleep(1 * time.Second)

		return c.NoContent(200)
	})

	e.Logger.Fatal(e.Start(":42069"))
}
func uploadImage(profilePicture *multipart.FileHeader) (string, string) {
    cdnAddress := "http://localhost:6969"
    // create a post request to the local cdn
    // make a post request with a multipart/form-data body containing the image
    file, err := profilePicture.Open()
    if err != nil {
        fmt.Println("Error opening file:", err)
        return "", ""
    }
    defer file.Close()

    var b bytes.Buffer
    w := multipart.NewWriter(&b)

    // Create form file field
    fw, err := w.CreateFormFile("file", profilePicture.Filename)
    if err != nil {
        fmt.Println("Error creating form file:", err)
        return "", ""
    }
    if _, err = io.Copy(fw, file); err != nil {
        fmt.Println("Error copying file:", err)
        return "", ""
    }

    // Add text field
    if err := w.WriteField("name", "test"); err != nil {
        fmt.Println("Error adding text field:", err)
        return "", ""
    }

    if err := w.Close(); err != nil {
        fmt.Println("Error closing writer:", err)
        return "", ""
    }

    req, err := http.NewRequest("POST", cdnAddress+"/form", &b)
    if err != nil {
        fmt.Println("Error creating request:", err)
        return "", ""
    }
    req.Header.Set("Content-Type", w.FormDataContentType())

    client := &http.Client{}
    resp, err := client.Do(req)
    if err != nil {
        fmt.Println("Error sending request:", err)
        return "", ""
    }
    defer resp.Body.Close()

    fmt.Println("Request sent successfully. Status Code:", resp.StatusCode)
    // get the response from the local cdn, the response is a JSON object containing the path and iconPath of the image
    var result map[string]string
    json.NewDecoder(resp.Body).Decode(&result)
    fmt.Println("response Body:", resp)
    fmt.Println(result)
    return cdnAddress+"/"+result["original"], cdnAddress+"/"+result["cached"]
}
