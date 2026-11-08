// resource.h - numeric IDs shared by the C++ code and app.rc.
#pragma once

#define IDC_STATIC                      -1

// ---- Resources -----------------------------------------------------------
#define IDI_APPICON                     101
#define IDR_MAINMENU                    102
#define IDR_ACCEL                       103
#define IDD_ADD_STUDENT                 110
#define IDD_ADD_COURSE                  111
#define IDD_ABOUT                       112

// ---- Menu commands -------------------------------------------------------
#define IDM_FILE_SAVE                   40001
#define IDM_FILE_EXIT                   40002
#define IDM_HELP_ABOUT                  40003

// ---- Dialog controls -----------------------------------------------------
#define IDC_STUDENT_ID                  1001
#define IDC_STUDENT_NAME                1002
#define IDC_COURSE_ID                   1011
#define IDC_COURSE_NAME                 1012
#define IDC_COURSE_CAPACITY             1013
#define IDC_COURSE_CAPACITY_SPIN        1014
#define IDC_DLG_ERROR                   1020

// ---- Main window controls ------------------------------------------------
#define IDC_TAB                         2001
#define IDC_STATUSBAR                   2002
#define IDC_PAGE_STUDENTS               2003
#define IDC_PAGE_COURSES                2004
#define IDC_PAGE_ENROLLMENT             2005

#define IDC_STU_SEARCH                  2011
#define IDC_STU_ADD                     2012
#define IDC_STU_LIST                    2013

#define IDC_CRS_SEARCH                  2021
#define IDC_CRS_ADD                     2022
#define IDC_CRS_LIST                    2023

#define IDC_ENR_STU_COMBO               2031
#define IDC_ENR_STU_LIST                2032
#define IDC_ENR_CRS_COMBO               2033
#define IDC_ENR_ENROLL                  2034
#define IDC_ENR_WITHDRAW                2035
#define IDC_ENR_VIEW_COMBO              2036
#define IDC_ENR_VIEW_SUMMARY            2037
#define IDC_ENR_VIEW_PROGRESS           2038
#define IDC_ENR_VIEW_LIST               2039
