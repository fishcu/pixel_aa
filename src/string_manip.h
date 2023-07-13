#include <string.h>

char* get_parent_path(const char* path) {
    char* parent_path = strdup(path);
    char* last_separator = strrchr(parent_path, '/');
    if (last_separator != NULL) {
        *last_separator = '\0';
    }
    return parent_path;
}

char* get_filename(const char* path) {
    const char* last_separator = strrchr(path, '/');
    if (last_separator != NULL) {
        return strdup(last_separator + 1);
    }
    return strdup(path);
}

char* remove_extension(const char* filename) {
    const char* extension_pos = strchr(filename, '.');
    if (extension_pos != NULL) {
        char* name_without_extension =
            (char*)malloc((extension_pos - filename + 1) * sizeof(char));
        strncpy(name_without_extension, filename, extension_pos - filename);
        name_without_extension[extension_pos - filename] = '\0';
        return name_without_extension;
    }
    return strdup(filename);
}

char* get_output_path(const char* directory, const char* output_file_name) {
    size_t directory_length = strlen(directory);
    size_t filename_length = strlen(output_file_name);
    const char* output_suffix = "_output.png";
    char* output_path = (char*)malloc(
        (directory_length + 1 + filename_length + strlen(output_suffix)) *
        sizeof(char));
    strcpy(output_path, directory);
    strcat(output_path, "/");
    strcat(output_path, output_file_name);
    strcat(output_path, output_suffix);
    return output_path;
}
