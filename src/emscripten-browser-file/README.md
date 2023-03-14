# Emscripten Browser File Library

Header-only C++ library to receive files from, and offer files to, the browser the Emscripten program is running in.  Compact implementation in a single header file.

Intended for use in Emscripten code, this enables the user to "upload" files to your program using their native file selector, and to "download" files from your program to save to disk, as if they were interacting with a remote website filesystem.

See also [tar_to_stream.h](https://github.com/Armchair-Software/tar_to_stream), to tarball multiple files in memory for a single download.

## Use cases:

* Implement an "upload" function, that enables users to choose a file using their browser's native file selector - this file is read directly into your program's memory.
  * Candidate files can be filtered as with an "accept" attribute, identical to [`<input>` elements with `type="file"`](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/file).
* Implement a "download" function, that allows you to create a "file" in memory, and offer it for "download" using the browser's native file-save dialogue.
  * Filename and MIME type can be specified.

## Functionality

* `emscripten_browser_file::download()` - your program shares a section of memory, and the user receives it as a file they can save
* `emscripten_browser_file::upload()` - user selects a file on their filesystem, and your program receives the contents in memory

### Download 

From the user's point of view, the `download` function acts as if the user has chosen to download a file from the web.  In this case, you define a buffer referencing data in memory and specify a filename and MIME type, and the user's browser either shows a "save as" interface asking where the file should be saved, or saves it to their default save location, as per their browser preferences.

#### Example

```cpp
#include <emscripten_browser_file.h>

auto main()->int {
  std::string filename{"hello_world.txt"};
  std::string mime_type{"application/text/plain"};
  std::string data{"Hello world!\n"};
  emscripten_browser_file::download(filename, mime_type, data);
}
```

The download call takes the following arguments:
```cpp
emscripten_browser_file::download(
  std::string const &filename,  // the default filename for the browser to save.  Note that browsers do not have to honour this, and may choose to mangle it
  std::string const &mime_type, // the MIME type of the data, treated as if it were a webserver serving a file
  std::string_view buffer       // a buffer describing the data to download - can be any array of bytes, passed as a string_view
) {
```

`download` also has an override accepting `std::string` instead of `char const*`.

For files containing binary data, you will usually want to use the MIME type `application/octet-stream`.

### Upload
From the user's point of view, the `upload` function acts as if the user is uploading a file to a remote website.  In this case, the file is loaded into a buffer in memory (referred to by a `std::string_view`) that is accessible to a C++ callback function you define.

#### Example

```cpp
#include <emscripten_browser_file.h>

void handle_upload_file(std::string const &filename, std::string const &mime_type, std::string_view buffer, void*) {
  // define a handler to process the file
  // ...
}

auto main()->int {
  // open the browser's file selector, and pass the file to the upload handler
  emscripten_browser_file::upload(".png,.jpg,.jpeg", handle_upload_file);
}

```

The upload call takes the following arguments:
```cpp
  emscripten_browser_file::upload(
    char const *accept_types,       // an "accept" attribute, listing what file types can be accepted - see: https://developer.mozilla.org/en-US/docs/Web/HTML/Element/input/file#unique_file_type_specifiers 
    upload_handler callback,        // a callback function to call with the received data
    void *callback_data = nullptr,  // optional pointer to pass to your callback function
  );
```
`upload` also has an override accepting `std::string` instead of `char const*`.

The callback must have the following signature:

```cpp
  void handle_upload_file(
    std::string const &filename,  // the filename of the file the user selected
    std::string const &mime_type, // the MIME type of the file the user selected, for example "image/png"
    std::string_view buffer,      // the file's content is exposed in this string_view - access the data with buffer.data() and size with buffer.size().
    void *callback_data = nullptr // optional callback data - identical to whatever you passed to handle_upload_file()
  );
```

#### Using callback data

The callback can receive additional data through a void pointer passed to the `upload` function:

```cpp
#include <emscripten_browser_file.h>
#include <iostream>

void handle_upload_file(std::string const &filename, std::string const &mime_type, std::string_view buffer, void *callback_data) {
  // define a handler to process the file
  auto my_data{*reintrepret_cast<std::string*>(my_data)};
  std::cout << "Received callback data: " << my_data << std::endl;
}

auto main()->int {
  std::string my_data{"hello world"};
  auto my_data_ptr{reintrepret_cast<void*>(&my_data)};

  // pass callback data to the handler
  emscripten_browser_file::upload(".png,.jpg,.jpeg", handle_upload_file, my_data_ptr);
}

```

You can use this to pass shared state, or any other data to the callback function - for example an instance of a class whose member function should be called to deal with the received data.

## Building

Necessary emsripten link flags:

- Building with emscripten will require you to pass, if you do not already do so, `-sEXPORTED_RUNTIME_METHODS=[ccall]` at the link stage.
- This uses dynamic memory allocation, so you need `-sALLOW_MEMORY_GROWTH=1` at the link stage.
- Depending on your optimisation settings, the compiler may remove JS `malloc` and `free` functions (this happens with `-O3` at the time of writing, see [emscripten issue 6882](https://github.com/emscripten-core/emscripten/issues/6882)).  This can be avoided by explicitly exporting those functions: add `-sEXPORTED_FUNCTIONS=[_main,_malloc,_free]` at the link stage.

## Other useful libraries

You may also find the following Emscripten helper libraries useful:

- [Emscripten Browser Clipboard Library](https://github.com/Armchair-Software/emscripten-browser-clipboard) - easy handling of browser copy and paste events in your C++ code.
- [Emscripten Browser Cursor](https://github.com/Armchair-Software/emscripten-browser-cursor) - easy manipulation of browser mouse pointer cursors from C++.
