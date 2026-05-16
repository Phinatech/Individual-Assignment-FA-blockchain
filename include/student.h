/* ============================================================
 *  student.h  --  Student registry interface
 *  Loads students from students.txt into an in-memory array
 *  and provides validation lookups used by the blockchain.
 * ============================================================ */
#ifndef STUDENT_H
#define STUDENT_H

#define MAX_STUDENTS     1024
#define STUDENT_ID_LEN   20
#define FULL_NAME_LEN    50
#define COURSE_CODE_LEN  10

typedef struct {
    char student_id[STUDENT_ID_LEN];
    char full_name[FULL_NAME_LEN];
    char course_code[COURSE_CODE_LEN];
} Student;

typedef struct {
    Student students[MAX_STUDENTS];
    int    count;
} StudentRegistry;

/* Load registry from CSV file.
 * Returns the number of students loaded, or -1 on error
 * (file missing, empty, or malformed). */
int registry_load(StudentRegistry *reg, const char *filename);

/* Linear-scan lookup by ID. NULL if not found. */
const Student *registry_find(const StudentRegistry *reg, const char *student_id);

/* Pretty-print the registry to stdout. */
void registry_print(const StudentRegistry *reg);

#endif /* STUDENT_H */
