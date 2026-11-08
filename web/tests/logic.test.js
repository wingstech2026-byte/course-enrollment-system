// Run with:  node --test web/tests
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const { EnrollmentSystem, statusOf } = require('../logic.js');
const storage = require('../storage.js');

const rejects = (result, fragment) => {
  assert.equal(result.ok, false, `expected rejection, got: ${result.message}`);
  if (fragment) assert.match(result.message, fragment);
};

test('students: validation rules', () => {
  const s = new EnrollmentSystem();
  assert.ok(s.addStudent('S001', 'Alice').ok);
  rejects(s.addStudent('S001', 'Dup'), /already exists/);
  rejects(s.addStudent('', 'X'), /must not be empty/);
  rejects(s.addStudent('S 2', 'X'), /spaces/);
  rejects(s.addStudent('S\t2', 'X'), /spaces/);
  rejects(s.addStudent('S|2', 'X'), /'\|'/);
  rejects(s.addStudent('S2', ''), /name must not be empty/);
  rejects(s.addStudent('S2', '   '), /name must not be empty/);
  rejects(s.addStudent('S2', 'A|B'), /'\|'/);
  assert.equal(s.addStudent('S2', 'A|B').field, 'name');
  assert.equal(s.addStudent('S 2', 'X').field, 'id');
  assert.equal(s.students.length, 1);
});

test('students: names are trimmed, unicode accepted', () => {
  const s = new EnrollmentSystem();
  assert.ok(s.addStudent('U1', '  José Ñandú 李雷  ').ok);
  assert.equal(s.students[0].name, 'José Ñandú 李雷');
});

test('courses: validation rules and shared IDs', () => {
  const s = new EnrollmentSystem();
  s.addStudent('S001', 'Alice');
  assert.ok(s.addCourse('S001', 'Shared ID with a student', '2').ok);
  assert.ok(s.addCourse('C1', 'Cap2', '2').ok);
  rejects(s.addCourse('C1', 'Dup', '5'), /already exists/);
  rejects(s.addCourse('C2', 'X', '0'), /greater than zero/);
  rejects(s.addCourse('C2', 'X', '000'), /greater than zero/);
  rejects(s.addCourse('C2', 'X', '-3'), /negative/);
  rejects(s.addCourse('C2', 'X', 'abc'), /digits only/);
  rejects(s.addCourse('C2', 'X', '2.5'), /digits only/);
  rejects(s.addCourse('C2', 'X', '1e3'), /digits only/);
  rejects(s.addCourse('C2', 'X', ''), /must not be empty/);
  rejects(s.addCourse('C2', 'X', '100001'), /exceed 100000/);
  rejects(s.addCourse('C2', 'X', '99999999999999999999'), /exceed 100000/);
  assert.ok(s.addCourse('C3', 'Max', '100000').ok);
  assert.equal(s.addCourse('C2', 'X', '0').field, 'capacity');
  rejects(s.addCourse('', 'X', '3'), /Course ID must not be empty/);
  rejects(s.addCourse('C 2', 'X', '3'), /Course ID must not contain spaces/);
  rejects(s.addCourse('C2', 'A|B', '3'), /Course name/);
});

test('enrollment: duplicate, capacity, nonexistent, withdraw', () => {
  const s = new EnrollmentSystem();
  ['S001', 'S002', 'S003'].forEach((id) => s.addStudent(id, id));
  s.addCourse('C1', 'Cap2', '2');

  assert.ok(s.enroll('S001', 'C1').ok);
  rejects(s.enroll('S001', 'C1'), /already enrolled/);
  assert.ok(s.enroll('S002', 'C1').ok);
  rejects(s.enroll('S003', 'C1'), /is full: all 2 places are taken/);
  rejects(s.enroll('S999', 'C1'), /Student 'S999' does not exist/);
  rejects(s.enroll('S001', 'NOPE'), /Course 'NOPE' does not exist/);
  assert.equal(s.enrolledCount('C1'), 2);
  assert.equal(s.remainingPlaces('C1'), 0);
  assert.equal(s.coursesOfStudent('S001').length, 1);
  assert.equal(s.studentsInCourse('C1').length, 2);

  rejects(s.withdraw('S003', 'C1'), /is not enrolled/);
  rejects(s.withdraw('S999', 'C1'), /does not exist/);
  assert.ok(s.withdraw('S002', 'C1').ok);
  assert.ok(s.enroll('S003', 'C1').ok, 'freed place can be reused');
  assert.equal(s.enrollments.length, 2);
});

test('capacity 1 uses singular wording', () => {
  const s = new EnrollmentSystem();
  s.addStudent('A', 'A'); s.addStudent('B', 'B');
  s.addCourse('C', 'One', '1');
  s.enroll('A', 'C');
  rejects(s.enroll('B', 'C'), /all 1 place is taken/);
});

test('modified flag', () => {
  const s = new EnrollmentSystem();
  assert.equal(s.modified, false);
  s.addStudent('A', 'A');
  assert.equal(s.modified, true);
  s.markSaved();
  assert.equal(s.modified, false);
  s.addStudent('A', 'dup'); // rejected: must not flag a change
  assert.equal(s.modified, false);
});

test('status thresholds: open < 80%, almost full >= 80%, full = 100%', () => {
  assert.equal(statusOf(0, 5), 'open');
  assert.equal(statusOf(3, 5), 'open');
  assert.equal(statusOf(4, 5), 'almost');
  assert.equal(statusOf(5, 5), 'full');
  assert.equal(statusOf(1, 4), 'open');
  assert.equal(statusOf(0, 1), 'open');
  assert.equal(statusOf(1, 1), 'full');
});

test('sample data loads cleanly: 6 students, 4 courses, 12 enrollments', () => {
  const s = new EnrollmentSystem();
  const report = storage.loadFromText(s, storage.SAMPLE);
  assert.deepEqual(report.warnings, []);
  assert.equal(s.students.length, 6);
  assert.equal(s.courses.length, 4);
  assert.equal(s.enrollments.length, 12);
  assert.equal(s.modified, false);
  assert.equal(s.remainingPlaces('CS101'), 0); // full
  assert.equal(s.remainingPlaces('CS201'), 1); // almost full
});

test('sample data equals the desktop SampleData files', () => {
  const dir = path.join(__dirname, '..', '..', 'SampleData');
  for (const name of ['students', 'courses', 'enrollments']) {
    const file = fs.readFileSync(path.join(dir, `${name}.txt`), 'utf8');
    assert.equal(storage.SAMPLE[name], file, `${name}.txt`);
  }
});

test('save format round-trips (including unicode)', () => {
  const s = new EnrollmentSystem();
  storage.loadFromText(s, storage.SAMPLE);
  s.addStudent('U1', 'José 李雷');
  const files = storage.serialize(s);
  const t = new EnrollmentSystem();
  const report = storage.loadFromText(t, files);
  assert.deepEqual(report.warnings, []);
  assert.deepEqual(t.students, s.students);
  assert.deepEqual(t.courses, s.courses);
  assert.deepEqual(t.enrollments, s.enrollments);
});

test('invalid files: 13 lines skipped, 2 students / 1 course / 1 enrollment loaded', () => {
  const dir = path.join(__dirname, '..', '..', 'SampleData', 'InvalidTest');
  const read = (n) => fs.readFileSync(path.join(dir, `${n}.txt`), 'utf8');
  const s = new EnrollmentSystem();
  const report = storage.loadFromText(s, {
    students: read('students'), courses: read('courses'), enrollments: read('enrollments'),
  });
  assert.equal(report.warnings.length, 13, report.warnings.join('\n'));
  assert.equal(s.students.length, 2);
  assert.equal(s.courses.length, 1);
  assert.equal(s.enrollments.length, 1);
  assert.match(report.warnings[0], /^students\.txt, line 2:/);
  assert.ok(report.warnings.some((w) => /is full: all 1 place is taken/.test(w)));
  assert.match(storage.summarize(report, 5), /and 8 more/);
});

test('loader tolerates BOM, blank lines and LF-only endings', () => {
  const s = new EnrollmentSystem();
  const report = storage.loadFromText(s, {
    students: '﻿S1|One\n\n   \nS2|Two\n',
    courses: 'C1|Course|3\n',
    enrollments: 'S1|C1\n',
  });
  assert.deepEqual(report.warnings, []);
  assert.equal(s.students.length, 2);
  assert.equal(s.enrollments.length, 1);
});

test('missing files mean empty data', () => {
  const s = new EnrollmentSystem();
  const report = storage.loadFromText(s, {});
  assert.deepEqual(report.warnings, []);
  assert.equal(s.students.length + s.courses.length + s.enrollments.length, 0);
});
