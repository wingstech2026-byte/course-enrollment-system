/*
 * storage.js - the three text-file formats (same as the desktop version) and
 * browser persistence.
 *
 *   students.txt     id|name
 *   courses.txt      id|name|capacity
 *   enrollments.txt  studentId|courseId
 *
 * Loading validates every line through EnrollmentSystem (the same rules as manual
 * entry); bad lines are skipped and reported in one summary.
 */
(function (root) {
  'use strict';

  const STORAGE_KEY = 'ems.v1';

  /** Demo data: 6 students, 4 courses (one full, one almost full), 12 enrollments. */
  const SAMPLE = {
    students: [
      'S001|Alice Johnson',
      'S002|Brian Smith',
      'S003|Chen Wei',
      'S004|Diana Lopez',
      'S005|Ethan Brown',
      'S006|Fatima Khan',
    ].join('\r\n') + '\r\n',
    courses: [
      'CS101|Introduction to Programming|5',
      'CS201|Data Structures|5',
      'MA101|Calculus I|30',
      'EN101|Academic Writing|4',
    ].join('\r\n') + '\r\n',
    enrollments: [
      'S001|CS101', 'S002|CS101', 'S003|CS101', 'S004|CS101', 'S005|CS101',
      'S001|CS201', 'S002|CS201', 'S003|CS201', 'S004|CS201',
      'S001|MA101', 'S006|MA101', 'S002|EN101',
    ].join('\r\n') + '\r\n',
  };

  /** Returns the non-blank lines of a file as [{ number, text }]. */
  function splitLines(text) {
    if (!text) return [];
    if (text.charCodeAt(0) === 0xfeff) text = text.slice(1); // UTF-8 byte order mark
    const lines = [];
    text.split('\n').forEach((raw, index) => {
      const line = raw.endsWith('\r') ? raw.slice(0, -1) : raw;
      if (line.trim().length > 0) lines.push({ number: index + 1, text: line });
    });
    return lines;
  }

  /**
   * Replaces the contents of `system` with the data in `files`
   * ({ students, courses, enrollments } strings; a missing file means "no data").
   * Returns a report: { warnings: string[], studentsLoaded, coursesLoaded, enrollmentsLoaded }.
   */
  function loadFromText(system, files) {
    const report = { warnings: [], studentsLoaded: 0, coursesLoaded: 0, enrollmentsLoaded: 0 };
    const skip = (file, line, reason) =>
      report.warnings.push(`${file}, line ${line.number}: ${reason}`);

    system.clear();

    for (const line of splitLines(files.students)) {
      const f = line.text.split('|');
      if (f.length !== 2) { skip('students.txt', line, 'malformed line (expected: id|name).'); continue; }
      const r = system.addStudent(f[0], f[1]);
      if (r.ok) report.studentsLoaded++; else skip('students.txt', line, r.message);
    }
    for (const line of splitLines(files.courses)) {
      const f = line.text.split('|');
      if (f.length !== 3) { skip('courses.txt', line, 'malformed line (expected: id|name|capacity).'); continue; }
      const r = system.addCourse(f[0], f[1], f[2]);
      if (r.ok) report.coursesLoaded++; else skip('courses.txt', line, r.message);
    }
    for (const line of splitLines(files.enrollments)) {
      const f = line.text.split('|');
      if (f.length !== 2) { skip('enrollments.txt', line, 'malformed line (expected: studentId|courseId).'); continue; }
      const r = system.enroll(f[0], f[1]);
      if (r.ok) report.enrollmentsLoaded++; else skip('enrollments.txt', line, r.message);
    }

    system.markSaved();
    return report;
  }

  /** Text of the three files for the current data (CRLF line endings, like the desktop app). */
  function serialize(system) {
    const join = (rows) => rows.map((r) => r + '\r\n').join('');
    return {
      students: join(system.students.map((s) => `${s.id}|${s.name}`)),
      courses: join(system.courses.map((c) => `${c.id}|${c.name}|${c.capacity}`)),
      enrollments: join(system.enrollments.map((e) => `${e.studentId}|${e.courseId}`)),
    };
  }

  /** One text listing at most `maxLines` warnings (for a single message box). */
  function summarize(report, maxLines) {
    let text = 'Some data in the files could not be loaded and was skipped:\n\n';
    report.warnings.slice(0, maxLines).forEach((w) => { text += `• ${w}\n`; });
    if (report.warnings.length > maxLines)
      text += `\n...and ${report.warnings.length - maxLines} more.`;
    return text;
  }

  /** Saves to this browser (localStorage). Returns { ok, message }. */
  function saveToBrowser(system) {
    try {
      root.localStorage.setItem(STORAGE_KEY, JSON.stringify(serialize(system)));
      return { ok: true, message: 'Saved to this browser.' };
    } catch (error) {
      return { ok: false, message: `The browser refused to store the data (${error.name}).` };
    }
  }

  /** Reads what was saved in this browser, or null if nothing (or unreadable). */
  function readFromBrowser() {
    try {
      const raw = root.localStorage.getItem(STORAGE_KEY);
      if (!raw) return null;
      const parsed = JSON.parse(raw);
      if (typeof parsed !== 'object' || parsed === null) return null;
      return {
        students: String(parsed.students || ''),
        courses: String(parsed.courses || ''),
        enrollments: String(parsed.enrollments || ''),
      };
    } catch (error) {
      return null;
    }
  }

  const api = { SAMPLE, splitLines, loadFromText, serialize, summarize, saveToBrowser, readFromBrowser };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.EMSStorage = api;
})(typeof self !== 'undefined' ? self : this);
