/* ============================================================
 *  student.c  --  Student registry implementation
 * ============================================================ */
#include "student.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* Strip leading/trailing whitespace from s in place. */
static char *trim(char *s)
{
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    return s;
}

/* Safely copy at most dst_sz-1 chars from src into dst and null terminate. */
static void safe_copy(char *dst, size_t dst_sz, const char *src)
{
    if (dst_sz == 0) return;
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

int registry_load(StudentRegistry *reg, const char *filename)
{
    if (!reg || !filename) return -1;
    reg->count = 0;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr,
                "ERROR: Could not open student registry '%s': %s\n",
                filename, strerror(errno));
        return -1;
    }

    char line[256];
    int line_no = 0;

    while (fgets(line, sizeof line, fp)) {
        line_no++;

        /* Skip blank lines and comments (#...) */
        char *trimmed = trim(line);
        if (*trimmed == '\0' || *trimmed == '#') continue;

        if (reg->count >= MAX_STUDENTS) {
            fprintf(stderr,
                    "WARN : Registry capacity (%d) reached; ignoring rest\n",
                    MAX_STUDENTS);
            break;
        }

        /* Parse: student_id,full_name,course_code  (commas as separators) */
        char *id    = strtok(trimmed, ",");
        char *name  = strtok(NULL, ",");
        char *code  = strtok(NULL, ",");

        if (!id || !name || !code) {
            fprintf(stderr,
                    "WARN : Malformed line %d in '%s' (skipped)\n",
                    line_no, filename);
            continue;
        }

        Student *s = &reg->students[reg->count];
        safe_copy(s->student_id,  STUDENT_ID_LEN,  trim(id));
        safe_copy(s->full_name,   FULL_NAME_LEN,   trim(name));
        safe_copy(s->course_code, COURSE_CODE_LEN, trim(code));
        reg->count++;
    }

    fclose(fp);

    if (reg->count == 0) {
        fprintf(stderr,
                "ERROR: Student registry '%s' is empty.\n", filename);
        return -1;
    }
    return reg->count;
}

const Student *registry_find(const StudentRegistry *reg, const char *student_id)
{
    if (!reg || !student_id) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->students[i].student_id, student_id) == 0)
            return &reg->students[i];
    }
    return NULL;
}

void registry_print(const StudentRegistry *reg)
{
    if (!reg) return;
    printf("\n+-----------------------------------------------------------------+\n");
    printf("| %-12s | %-30s | %-10s |\n",
           "STUDENT ID", "FULL NAME", "COURSE");
    printf("+-----------------------------------------------------------------+\n");
    for (int i = 0; i < reg->count; i++) {
        printf("| %-12s | %-30s | %-10s |\n",
               reg->students[i].student_id,
               reg->students[i].full_name,
               reg->students[i].course_code);
    }
    printf("+-----------------------------------------------------------------+\n");
    printf("Total: %d student(s).\n\n", reg->count);
}
