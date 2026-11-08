/*
 * logic.js - business rules of the Course Enrollment Management System.
 *
 * A direct port of EnrollmentSystem.cpp. It contains NO user-interface code:
 * every mutating method validates its input and returns
 *   { ok: boolean, message: string, field: 'id' | 'name' | 'capacity' | null }
 * so the rules are enforced no matter who calls them (dialogs, drop-downs, file import).
 */
(function (root) {
  'use strict';

  /** Upper bound for a course capacity (same limit as the desktop version). */
  const MAX_CAPACITY = 100000;

  const ok = (message) => ({ ok: true, message, field: null });
  const fail = (message, field = null) => ({ ok: false, message, field });

  const isSpace = (ch) => /\s/.test(ch);
  const isControl = (ch) => /[\u0000-\u001f\u007f-\u009f]/.test(ch);

  /** Checks the syntax of an ID (uniqueness is checked by the caller). */
  function validateId(id, label) {
    if (id.length === 0) return fail(`${label} must not be empty.`, 'id');
    for (const ch of id) {
      if (isSpace(ch)) return fail(`${label} must not contain spaces.`, 'id');
      if (ch === '|') return fail(`${label} must not contain the '|' character.`, 'id');
      if (isControl(ch)) return fail(`${label} must not contain control characters.`, 'id');
    }
    return ok('');
  }

  /** Checks a name. On success `cleaned` holds the trimmed name. */
  function validateName(name, label) {
    const cleaned = name.trim();
    if (cleaned.length === 0) return fail(`${label} must not be empty.`, 'name');
    for (const ch of cleaned) {
      if (ch === '|') return fail(`${label} must not contain the '|' character.`, 'name');
      if (isControl(ch)) return fail(`${label} must not contain control characters.`, 'name');
    }
    return { ok: true, message: '', field: null, cleaned };
  }

  /** Parses capacity text. Only a positive whole number (1..MAX_CAPACITY) passes. */
  function parseCapacity(rawText) {
    const text = String(rawText).trim();
    if (text.length === 0)
      return fail('Capacity must not be empty. Enter a positive whole number.', 'capacity');

    const negative = text.startsWith('-');
    const digits = negative ? text.slice(1) : text;
    if (!/^[0-9]+$/.test(digits))
      return fail('Capacity must be a whole number (digits only).', 'capacity');

    if (negative)
      return fail('Capacity must not be negative. Enter a positive whole number.', 'capacity');

    const significant = digits.replace(/^0+/, '');
    if (significant.length === 0) return fail('Capacity must be greater than zero.', 'capacity');
    if (significant.length > 6 || Number(significant) > MAX_CAPACITY)
      return fail(`Capacity must not exceed ${MAX_CAPACITY}.`, 'capacity');

    return { ok: true, message: '', field: null, value: Number(significant) };
  }

  /** "S001 (Alice)" style description used in messages. */
  const describe = (id, name) => `${id} (${name})`;

  class EnrollmentSystem {
    constructor() {
      this.clear();
    }

    /** Removes all data and resets the modified flag. */
    clear() {
      this.students = []; // { id, name }
      this.courses = []; // { id, name, capacity }
      this.enrollments = []; // { studentId, courseId }
      this.modified = false;
    }

    markSaved() {
      this.modified = false;
    }

    // ---- Mutating operations --------------------------------------------

    addStudent(id, name) {
      id = String(id);
      let check = validateId(id, 'Student ID');
      if (!check.ok) return check;
      check = validateName(String(name), 'Student name');
      if (!check.ok) return check;
      if (this.findStudent(id))
        return fail(`Student ID '${id}' already exists. Student IDs must be unique.`, 'id');

      this.students.push({ id, name: check.cleaned });
      this.modified = true;
      return ok(`Student added: ${describe(id, check.cleaned)}.`);
    }

    addCourse(id, name, capacityText) {
      id = String(id);
      let check = validateId(id, 'Course ID');
      if (!check.ok) return check;
      const named = validateName(String(name), 'Course name');
      if (!named.ok) return named;
      const cap = parseCapacity(capacityText);
      if (!cap.ok) return cap;
      if (this.findCourse(id))
        return fail(`Course ID '${id}' already exists. Course IDs must be unique.`, 'id');

      this.courses.push({ id, name: named.cleaned, capacity: cap.value });
      this.modified = true;
      return ok(`Course added: ${describe(id, named.cleaned)}, capacity ${cap.value}.`);
    }

    enroll(studentId, courseId) {
      const student = this.findStudent(studentId);
      if (!student) return fail(`Student '${studentId}' does not exist.`);
      const course = this.findCourse(courseId);
      if (!course) return fail(`Course '${courseId}' does not exist.`);

      if (this.isEnrolled(studentId, courseId))
        return fail(
          `Student ${describe(student.id, student.name)} is already enrolled in course ` +
            `${describe(course.id, course.name)}.`
        );

      if (this.enrolledCount(courseId) >= course.capacity)
        return fail(
          `Course ${describe(course.id, course.name)} is full: all ${course.capacity} ` +
            `${course.capacity === 1 ? 'place is' : 'places are'} taken.`
        );

      this.enrollments.push({ studentId, courseId });
      this.modified = true;
      return ok(
        `Enrolled ${describe(student.id, student.name)} in ${describe(course.id, course.name)}. ` +
          `Places remaining: ${this.remainingPlaces(courseId)}.`
      );
    }

    withdraw(studentId, courseId) {
      const student = this.findStudent(studentId);
      if (!student) return fail(`Student '${studentId}' does not exist.`);
      const course = this.findCourse(courseId);
      if (!course) return fail(`Course '${courseId}' does not exist.`);

      const index = this.enrollments.findIndex(
        (e) => e.studentId === studentId && e.courseId === courseId
      );
      if (index < 0)
        return fail(
          `Student ${describe(student.id, student.name)} is not enrolled in course ` +
            `${describe(course.id, course.name)}.`
        );

      this.enrollments.splice(index, 1);
      this.modified = true;
      return ok(
        `Withdrew ${describe(student.id, student.name)} from ${describe(course.id, course.name)}.`
      );
    }

    // ---- Queries ---------------------------------------------------------

    findStudent(id) {
      return this.students.find((s) => s.id === id) || null;
    }

    findCourse(id) {
      return this.courses.find((c) => c.id === id) || null;
    }

    isEnrolled(studentId, courseId) {
      return this.enrollments.some((e) => e.studentId === studentId && e.courseId === courseId);
    }

    enrolledCount(courseId) {
      return this.enrollments.filter((e) => e.courseId === courseId).length;
    }

    remainingPlaces(courseId) {
      const course = this.findCourse(courseId);
      return course ? Math.max(0, course.capacity - this.enrolledCount(courseId)) : 0;
    }

    courseCountOfStudent(studentId) {
      return this.enrollments.filter((e) => e.studentId === studentId).length;
    }

    /** Courses selected by a student (function 5), in enrollment order. */
    coursesOfStudent(studentId) {
      return this.enrollments
        .filter((e) => e.studentId === studentId)
        .map((e) => this.findCourse(e.courseId))
        .filter(Boolean);
    }

    /** Students enrolled in a course (function 6), in enrollment order. */
    studentsInCourse(courseId) {
      return this.enrollments
        .filter((e) => e.courseId === courseId)
        .map((e) => this.findStudent(e.studentId))
        .filter(Boolean);
    }
  }

  /** Open / Almost full (>= 80 %) / Full. Returns 'open' | 'almost' | 'full'. */
  function statusOf(enrolled, capacity) {
    if (enrolled >= capacity) return 'full';
    if (enrolled * 100 >= capacity * 80) return 'almost';
    return 'open';
  }

  const api = { EnrollmentSystem, MAX_CAPACITY, statusOf };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.EMSLogic = api;
})(typeof self !== 'undefined' ? self : this);
