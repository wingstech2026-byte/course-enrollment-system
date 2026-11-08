/*
 * app.js - user interface of the browser edition.
 * All user-supplied text is inserted with textContent / text nodes (never innerHTML).
 */
(function () {
  'use strict';

  const { EnrollmentSystem, MAX_CAPACITY, statusOf } = window.EMSLogic;
  const storage = window.EMSStorage;

  const TITLE = 'Course Enrollment Management System';
  const SEAT_MAP_LIMIT = 60; // courses up to this many seats get a seat map
  const SEGMENTED_METER_LIMIT = 12; // up to this many seats the table meter shows one block per seat
  const $ = (selector) => document.querySelector(selector);
  const collator = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

  const system = new EnrollmentSystem();
  let lastAction = 'Ready.';
  let lastKind = 'info'; // 'ok' | 'error' | 'info'
  let courseFilter = 'all'; // 'all' | 'open' | 'almost' | 'full'
  let animateSeats = false;

  const STATUS_LABEL = { open: 'Open', almost: 'Almost full', full: 'Full' };

  /** Tiny DOM builder: h('tr', { class: 'x', onclick: fn }, child, 'text'). */
  function h(tag, props, ...children) {
    const el = document.createElement(tag);
    for (const [key, value] of Object.entries(props || {})) {
      if (value === false || value == null) continue;
      if (key === 'class') el.className = value;
      else if (key.startsWith('on')) el.addEventListener(key.slice(2), value);
      else el.setAttribute(key, value === true ? '' : value);
    }
    for (const child of children.flat()) el.append(child);
    return el;
  }

  /** <svg><use href="#i-name"/></svg> from the sprite in index.html. */
  function icon(name) {
    const NS = 'http://www.w3.org/2000/svg';
    const svg = document.createElementNS(NS, 'svg');
    svg.setAttribute('class', 'i');
    svg.setAttribute('aria-hidden', 'true');
    const use = document.createElementNS(NS, 'use');
    use.setAttribute('href', `#i-${name}`);
    svg.append(use);
    return svg;
  }

  const plural = (n, one, many) => `${n} ${n === 1 ? one : many}`;

  // ===========================================================================
  // Small visual building blocks
  // ===========================================================================

  /** First letter of the first and last word ("Alice Johnson" -> "AJ"). */
  function initials(name) {
    const words = name.trim().split(/\s+/).filter(Boolean);
    if (words.length === 0) return '?';
    const first = [...words[0]][0];
    const last = words.length > 1 ? [...words[words.length - 1]][0] : '';
    return (first + last).toUpperCase();
  }

  /** A stable hue (0-359) derived from an ID, so a person always gets the same color. */
  function hueOf(id) {
    let n = 0;
    for (const ch of id) n = (n * 31 + ch.codePointAt(0)) % 100003;
    // Neighbouring IDs (S001, S002 ...) differ by 1, so step by the golden angle to spread the hues apart.
    return Math.round((n * 137.508) % 360);
  }

  function avatar(name, id, large) {
    const el = h('span', { class: `avatar${large ? ' lg' : ''}`, 'aria-hidden': 'true' }, initials(name));
    el.style.setProperty('--h', String(hueOf(id)));
    return el;
  }

  const pill = (status) => h('span', { class: `pill ${status}` }, STATUS_LABEL[status]);

  /** Seat meter for the courses table: one block per seat for small courses, a bar otherwise. */
  function seatMeter(enrolled, capacity, status) {
    const label = `${enrolled} of ${capacity} seats taken`;
    if (capacity <= SEGMENTED_METER_LIMIT) {
      const wrap = h('span', { class: `segmeter ${status}`, role: 'img', 'aria-label': label });
      for (let i = 0; i < capacity; i++) wrap.append(h('i', { class: i < enrolled ? 'on' : '' }));
      return wrap;
    }
    const percent = Math.min(100, (enrolled / capacity) * 100);
    const bar = h('span', { class: `meter ${status}`, role: 'img', 'aria-label': label }, h('i'));
    bar.firstElementChild.style.width = `${percent}%`;
    return bar;
  }

  // ===========================================================================
  // Sortable tables
  // ===========================================================================

  /**
   * columns: [{ title, render?(row) }]. A row is { key, cells: string[], tag?, data? };
   * `cells` are the sort keys and the default text, `render` may draw something richer.
   */
  function makeTable(selector, columns, options = {}) {
    const table = {
      el: $(selector), columns, rows: [], sort: { col: 0, asc: true },
      selectable: !!options.selectable, selectedKey: null, onSelect: options.onSelect || (() => {}),
      empty: options.empty || (() => 'Nothing to show.'),
    };
    const headRow = h('tr');
    columns.forEach((column, index) => {
      const button = h('button', { type: 'button', title: `Sort by ${column.title.toLowerCase()}` },
        column.title, h('span', { class: 'arrow', 'aria-hidden': 'true' }, '▲'));
      button.addEventListener('click', () => {
        if (table.sort.col === index) table.sort.asc = !table.sort.asc;
        else table.sort = { col: index, asc: true };
        paint(table);
      });
      headRow.append(h('th', { scope: 'col', 'aria-sort': 'none' }, button));
    });
    table.el.tHead.append(headRow);
    return table;
  }

  /** Sorts the rows of `table` by its sort state and redraws the body. */
  function paint(table) {
    const { col, asc } = table.sort;
    table.rows.sort((a, b) => (asc ? 1 : -1) * collator.compare(a.cells[col], b.cells[col]));

    table.el.tHead.querySelectorAll('th').forEach((th, index) => {
      th.setAttribute('aria-sort', index === col ? (asc ? 'ascending' : 'descending') : 'none');
      th.querySelector('.arrow').textContent = asc ? '▲' : '▼';
    });

    const body = table.el.tBodies[0];
    body.replaceChildren();
    if (table.rows.length === 0) {
      body.append(h('tr', { class: 'empty' }, h('td', { colspan: table.columns.length }, table.empty())));
      return;
    }
    for (const row of table.rows) {
      const cells = table.columns.map((column, index) => h('td', {}, column.render ? column.render(row) : row.cells[index]));
      const tr = h('tr', { class: row.tag ? `row-${row.tag}` : '' }, cells);
      tr.dataset.key = row.key;
      if (table.selectable) {
        tr.tabIndex = 0;
        tr.setAttribute('aria-selected', String(row.key === table.selectedKey));
        const choose = () => { table.selectedKey = row.key; paint(table); table.onSelect(row.key); focusRow(table, row.key); };
        tr.addEventListener('click', choose);
        tr.addEventListener('keydown', (event) => {
          if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); choose(); }
          else if (event.key === 'ArrowDown' && tr.nextElementSibling) tr.nextElementSibling.focus();
          else if (event.key === 'ArrowUp' && tr.previousElementSibling) tr.previousElementSibling.focus();
        });
      }
      body.append(tr);
    }
  }

  function focusRow(table, key) {
    const tr = [...table.el.tBodies[0].rows].find((r) => r.dataset.key === key);
    if (tr) tr.focus();
  }

  /** Replaces the rows of `table`, keeping the selection if that row still exists. */
  function setRows(table, rows) {
    table.rows = rows;
    if (table.selectedKey !== null && !rows.some((r) => r.key === table.selectedKey)) table.selectedKey = null;
    paint(table);
  }

  const idCell = (row) => h('span', { class: 'id' }, row.cells[0]);
  const personCell = (row) => h('div', { class: 'person-cell' }, avatar(row.cells[1], row.key), h('span', {}, row.cells[1]));

  /** Empty-state block: a plain statement and, when it helps, the action that fixes it. */
  const emptyState = (title, detail, actionLabel, action) =>
    h('div', {}, h('strong', {}, title), detail, actionLabel ? h('div', {}, h('button', { type: 'button', class: 'btn primary', onclick: action }, actionLabel)) : '');

  const tblStudents = makeTable('#tbl-students', [
    { title: 'Student ID', render: idCell },
    { title: 'Name', render: personCell },
    {
      title: 'Courses Taken',
      render: (row) => {
        const ids = row.data.courses;
        return h('div', { class: 'taken' }, h('span', { class: 'num' }, row.cells[2]),
          ids.slice(0, 3).map((id) => h('span', { class: 'chip' }, id)),
          ids.length > 3 ? h('span', { class: 'chip' }, `+${ids.length - 3}`) : '');
      },
    },
  ], {
    empty: () => system.students.length === 0
      ? emptyState('No students yet', 'Add the first student to start enrolling.', 'Add student', () => openStudentDialog())
      : `No student matches “${$('#stu-search').value.trim()}”.`,
  });

  const tblCourses = makeTable('#tbl-courses', [
    { title: 'Course ID', render: idCell },
    { title: 'Name' },
    { title: 'Capacity' },
    { title: 'Enrolled', render: (row) => h('div', { class: 'enrolled' }, h('span', { class: 'num' }, row.cells[3]), seatMeter(row.data.enrolled, row.data.capacity, row.tag)) },
    { title: 'Remaining' },
    { title: 'Status', render: (row) => pill(row.tag) },
  ], {
    empty: () => system.courses.length === 0
      ? emptyState('No courses yet', 'Add a course and set how many students it can hold.', 'Add course', () => openCourseDialog())
      : 'No course matches the current search and filter.',
  });

  const tblStudentCourses = makeTable('#tbl-student-courses', [
    { title: 'Course ID', render: idCell },
    { title: 'Course Name' },
    { title: 'Seats', render: (row) => h('div', { class: 'enrolled' }, h('span', { class: 'num' }, row.cells[2]), seatMeter(row.data.enrolled, row.data.capacity, row.tag)) },
  ], {
    selectable: true, onSelect: () => updateButtons(),
    empty: () => $('#enr-student').value ? 'Not enrolled in any course yet. Choose a course below and press Enroll.' : 'Choose a student to see their courses.',
  });

  const tblCourseStudents = makeTable('#tbl-course-students', [
    { title: 'Student ID', render: idCell },
    { title: 'Student Name', render: personCell },
  ], {
    empty: () => $('#enr-view-course').value ? 'No one is enrolled in this course yet.' : 'Choose a course to see its students.',
  });

  // ===========================================================================
  // Refreshing the display
  // ===========================================================================

  const matches = (filter, ...texts) => !filter || texts.some((t) => t.toLowerCase().includes(filter));

  function refreshStudents() {
    const filter = $('#stu-search').value.trim().toLowerCase();
    const rows = system.students
      .filter((s) => matches(filter, s.id, s.name))
      .map((s) => {
        const courses = system.coursesOfStudent(s.id).map((c) => c.id);
        return { key: s.id, cells: [s.id, s.name, String(courses.length)], data: { courses } };
      });
    setRows(tblStudents, rows);
    $('#stu-count').textContent = filter
      ? `Showing ${rows.length} of ${plural(system.students.length, 'student', 'students')}`
      : plural(system.students.length, 'student', 'students');
  }

  function refreshCourses() {
    const filter = $('#crs-search').value.trim().toLowerCase();
    const all = system.courses.map((c) => {
      const enrolled = system.enrolledCount(c.id);
      return { course: c, enrolled, status: statusOf(enrolled, c.capacity) };
    });
    const searched = all.filter((x) => matches(filter, x.course.id, x.course.name));

    // Status filter buttons with counts (counts respect the search box).
    const counts = { all: searched.length, open: 0, almost: 0, full: 0 };
    searched.forEach((x) => { counts[x.status]++; });
    $('#crs-filter').replaceChildren(...['all', 'open', 'almost', 'full'].map((name) =>
      h('button', {
        type: 'button', 'aria-pressed': String(courseFilter === name),
        onclick: () => { courseFilter = name; refreshCourses(); },
      }, name === 'all' ? 'All' : STATUS_LABEL[name], h('span', { class: 'n' }, String(counts[name])))));

    const shown = searched.filter((x) => courseFilter === 'all' || x.status === courseFilter);
    setRows(tblCourses, shown.map(({ course: c, enrolled, status }) => ({
      key: c.id, tag: status, data: { enrolled, capacity: c.capacity },
      cells: [c.id, c.name, String(c.capacity), String(enrolled), String(Math.max(0, c.capacity - enrolled)), STATUS_LABEL[status]],
    })));
    $('#crs-count').textContent = shown.length === system.courses.length
      ? plural(system.courses.length, 'course', 'courses')
      : `Showing ${shown.length} of ${plural(system.courses.length, 'course', 'courses')}`;
  }

  /** Refills a <select>, keeping the previous choice when it still exists. */
  function fillSelect(select, placeholder, options) {
    const previous = select.value;
    select.replaceChildren(h('option', { value: '' }, placeholder));
    for (const o of options) select.append(h('option', { value: o.value }, o.label));
    select.value = options.some((o) => o.value === previous) ? previous : '';
  }

  function refreshCombos() {
    const students = [...system.students].sort((a, b) => collator.compare(a.id, b.id));
    const courses = [...system.courses].sort((a, b) => collator.compare(a.id, b.id));
    const courseOptions = courses.map((c) => {
      const left = system.remainingPlaces(c.id);
      return { value: c.id, label: `${c.id} - ${c.name}  (${left > 0 ? `${left} left` : 'full'})` };
    });
    fillSelect($('#enr-student'), 'Choose a student', students.map((s) => ({ value: s.id, label: `${s.id} - ${s.name}` })));
    fillSelect($('#enr-course'), 'Choose a course', courseOptions);
    fillSelect($('#enr-view-course'), 'Choose a course', courseOptions);
  }

  function refreshStudentCourses() {
    const studentId = $('#enr-student').value;
    const student = studentId ? system.findStudent(studentId) : null;
    const card = $('#student-card');

    if (!student) {
      card.hidden = true;
      setRows(tblStudentCourses, []);
      return;
    }
    const courses = system.coursesOfStudent(studentId);
    card.replaceChildren(
      avatar(student.name, student.id, true),
      h('div', { class: 'who' }, h('strong', {}, student.name), h('span', {}, student.id)),
      h('span', { class: 'pill' }, plural(courses.length, 'course', 'courses')));
    card.hidden = false;

    setRows(tblStudentCourses, courses.map((c) => {
      const enrolled = system.enrolledCount(c.id);
      return {
        key: c.id, tag: statusOf(enrolled, c.capacity), data: { enrolled, capacity: c.capacity },
        cells: [c.id, c.name, `${enrolled} / ${c.capacity}`],
      };
    }));
  }

  function refreshCourseStudents() {
    const courseId = $('#enr-view-course').value;
    const course = courseId ? system.findCourse(courseId) : null;
    const summary = $('#enr-summary');
    const bar = $('#enr-bar');
    const seatmap = $('#seatmap');
    const head = $('#course-head');
    const legend = $('#seat-legend');

    if (!course) {
      head.hidden = true;
      seatmap.hidden = true;
      legend.hidden = true;
      bar.hidden = true;
      summary.textContent = 'Select a course to see its enrolled students.';
      setRows(tblCourseStudents, []);
      return;
    }

    const roster = system.studentsInCourse(courseId);
    const enrolled = roster.length;
    const remaining = system.remainingPlaces(courseId);
    const status = statusOf(enrolled, course.capacity);

    head.replaceChildren(
      h('div', { class: 'who' }, h('strong', {}, course.name), h('span', { class: 'sub' }, course.id)),
      pill(status));
    head.hidden = false;
    summary.textContent = `Enrolled ${enrolled} / ${course.capacity} — ${plural(remaining, 'place', 'places')} remaining`;

    if (course.capacity <= SEAT_MAP_LIMIT) {
      bar.hidden = true;
      seatmap.className = `seatmap ${status}${animateSeats ? ' animate' : ''}`;
      seatmap.setAttribute('aria-label', `Seat map: ${enrolled} of ${course.capacity} seats taken`);
      seatmap.replaceChildren(...Array.from({ length: course.capacity }, (_, i) => {
        const student = roster[i];
        const seat = student
          ? h('span', { class: 'seat on', title: `${student.id} ${student.name}`, 'aria-hidden': 'true' }, initials(student.name))
          : h('span', { class: 'seat off', title: 'Open seat', 'aria-hidden': 'true' });
        seat.style.setProperty('--i', String(i));
        return seat;
      }));
      seatmap.hidden = false;
      legend.className = `seat-legend ${status}`;
      legend.hidden = false;
    } else {
      // Too many seats to draw one by one: fall back to a bar.
      seatmap.hidden = true;
      legend.hidden = true;
      const percent = Math.min(100, Math.round((enrolled / course.capacity) * 100));
      bar.className = `meter ${status}`;
      bar.firstElementChild.style.width = `${percent}%`;
      bar.setAttribute('aria-valuenow', String(percent));
      bar.hidden = false;
    }
    animateSeats = false;
    setRows(tblCourseStudents, roster.map((s) => ({ key: s.id, cells: [s.id, s.name] })));
  }

  function updateButtons() {
    const studentId = $('#enr-student').value;
    $('#btn-enroll').disabled = !(studentId && $('#enr-course').value);
    $('#btn-withdraw').disabled = !(studentId && tblStudentCourses.selectedKey !== null);
  }

  function updateSidebar() {
    $('#nav-students').textContent = String(system.students.length);
    $('#nav-courses').textContent = String(system.courses.length);
    $('#nav-enrollments').textContent = String(system.enrollments.length);

    const seats = system.courses.reduce((sum, c) => sum + c.capacity, 0);
    const taken = system.enrollments.length;
    $('#usage-text').textContent = seats === 0 ? 'No seats defined yet' : `${taken} of ${plural(seats, 'seat', 'seats')} taken`;
    $('#usage-bar').style.width = seats === 0 ? '0%' : `${Math.min(100, (taken / seats) * 100)}%`;
  }

  function updateStatusBar() {
    $('#st-students').textContent = plural(system.students.length, 'student', 'students');
    $('#st-courses').textContent = plural(system.courses.length, 'course', 'courses');
    $('#st-enrollments').textContent = plural(system.enrollments.length, 'enrollment', 'enrollments');
    const last = $('#st-last');
    last.textContent = lastAction;
    last.dataset.kind = lastKind;
  }

  function updateTitle() {
    document.title = system.modified ? `${TITLE} *` : TITLE;
    $('#unsaved').hidden = !system.modified;
  }

  function setStatus(text, kind = 'info') {
    lastAction = text;
    lastKind = kind;
    updateStatusBar();
  }

  /** Every list, drop-down and counter is refreshed after any change. */
  function refreshAll() {
    refreshStudents();
    refreshCourses();
    refreshCombos();
    refreshStudentCourses();
    refreshCourseStudents();
    updateButtons();
    updateSidebar();
    updateStatusBar();
    updateTitle();
  }

  // ===========================================================================
  // Message / confirmation dialog (the browser version of MessageBox)
  // ===========================================================================

  const MESSAGE_ICON = { error: 'error', warning: 'warning', question: 'help', info: 'info' };

  /**
   * Shows a modal message. Resolves with the `value` of the chosen button, or 'cancel'
   * when dismissed with Esc.
   */
  function showMessage({ title, text, kind = 'info', buttons = [{ label: 'OK', value: 'ok', primary: true }], focus = 0 }) {
    const dialog = $('#dlg-message');
    $('#msg-title').textContent = title;
    $('#msg-text').textContent = text;
    dialog.querySelector('.msg').className = `msg ${kind}`;
    $('#msg-icon').replaceChildren(icon(MESSAGE_ICON[kind]));

    const row = $('#msg-buttons');
    row.replaceChildren();
    buttons.forEach((b) => row.append(
      h('button', { type: 'button', class: `btn${b.primary ? ' primary' : ''}`, onclick: () => dialog.close(b.value) }, b.label)
    ));

    dialog.returnValue = '';
    return new Promise((resolve) => {
      dialog.addEventListener('close', () => resolve(dialog.returnValue || 'cancel'), { once: true });
      dialog.showModal();
      row.children[focus].focus();
    });
  }

  const showError = (title, text) => showMessage({ title, text, kind: 'error' });
  const confirmYesNo = async (title, text) =>
    (await showMessage({
      title, text, kind: 'question',
      buttons: [{ label: 'Yes', value: 'yes', primary: true }, { label: 'No', value: 'no' }],
      focus: 1, // default answer is "No"
    })) === 'yes';

  // ===========================================================================
  // Add student / Add course dialogs (inline validation, dialog stays open on errors)
  // ===========================================================================

  /** Wires a dialog and returns the function that opens it. */
  function wireAddDialog({ dialog, form, errorEl, fields, openButton, submit }) {
    const open = () => {
      form.reset();
      errorEl.textContent = '';
      dialog.showModal();
      fields.id.focus();
    };
    openButton.addEventListener('click', open);
    form.querySelector('[data-cancel]').addEventListener('click', () => dialog.close('cancel'));
    form.addEventListener('submit', (event) => {
      event.preventDefault();
      const result = submit();
      if (!result.ok) {
        errorEl.textContent = result.message;
        const target = fields[result.field] || fields.id;
        target.focus();
        target.select();
        return; // keep the dialog open
      }
      dialog.close('ok');
      refreshAll();
      setStatus(result.message, 'ok');
    });
    return open;
  }

  const openStudentDialog = wireAddDialog({
    dialog: $('#dlg-student'), form: $('#form-student'), errorEl: $('#stu-error'),
    fields: { id: $('#stu-id'), name: $('#stu-name') }, openButton: $('#stu-add'),
    submit: () => system.addStudent($('#stu-id').value, $('#stu-name').value),
  });

  const openCourseDialog = wireAddDialog({
    dialog: $('#dlg-course'), form: $('#form-course'), errorEl: $('#crs-error'),
    fields: { id: $('#crs-id'), name: $('#crs-name'), capacity: $('#crs-cap') }, openButton: $('#crs-add'),
    submit: () => {
      const cap = $('#crs-cap');
      // A number box reports "" for text such as "abc": hand a non-numeric text to the rules instead.
      const text = cap.validity.badInput ? 'not a number' : cap.value;
      return system.addCourse($('#crs-id').value, $('#crs-name').value, text);
    },
  });

  document.querySelectorAll('#form-course [data-step]').forEach((button) =>
    button.addEventListener('click', () => {
      const input = $('#crs-cap');
      const current = parseInt(input.value, 10);
      const base = Number.isFinite(current) ? current : 30;
      input.value = String(Math.min(MAX_CAPACITY, Math.max(1, base + Number(button.dataset.step))));
    }));

  // ===========================================================================
  // Enroll / withdraw
  // ===========================================================================

  async function onEnroll() {
    const result = system.enroll($('#enr-student').value, $('#enr-course').value);
    if (!result.ok) {
      setStatus('Enrollment rejected.', 'error');
      await showError('Cannot enroll', result.message);
      return;
    }
    refreshAll();
    setStatus(result.message, 'ok');
  }

  async function onWithdraw() {
    const studentId = $('#enr-student').value;
    const courseId = tblStudentCourses.selectedKey;
    if (!studentId || courseId === null) return;

    const student = system.findStudent(studentId);
    const course = system.findCourse(courseId);
    const question = student && course
      ? `Withdraw ${student.name} (${student.id}) from ${course.name} (${course.id})?`
      : 'Withdraw this student from the course?';
    if (!(await confirmYesNo('Confirm withdrawal', question))) return;

    const result = system.withdraw(studentId, courseId);
    if (!result.ok) {
      setStatus('Withdrawal rejected.', 'error');
      await showError('Cannot withdraw', result.message);
      return;
    }
    refreshAll();
    setStatus(result.message, 'ok');
    $('#enr-student').focus(); // the Withdraw button just became disabled
  }

  $('#enr-student').addEventListener('change', () => { tblStudentCourses.selectedKey = null; refreshStudentCourses(); updateButtons(); });
  $('#enr-course').addEventListener('change', updateButtons);
  $('#enr-view-course').addEventListener('change', () => { animateSeats = true; refreshCourseStudents(); });
  $('#btn-enroll').addEventListener('click', onEnroll);
  $('#btn-withdraw').addEventListener('click', onWithdraw);
  $('#stu-search').addEventListener('input', refreshStudents);
  $('#crs-search').addEventListener('input', refreshCourses);

  // ===========================================================================
  // Save / load / import / export
  // ===========================================================================

  function saveData() {
    const result = storage.saveToBrowser(system);
    if (!result.ok) {
      setStatus('Save failed.', 'error');
      showError('Cannot save', result.message);
      return false;
    }
    system.markSaved();
    updateTitle();
    setStatus(`Saved ${plural(system.students.length, 'student', 'students')}, ${plural(system.courses.length, 'course', 'courses')}, ` +
      `${plural(system.enrollments.length, 'enrollment', 'enrollments')} in this browser.`, 'ok');
    return true;
  }

  /** Loads file texts into the system and reports problems in one warning. */
  async function applyFiles(files, { modified, origin }) {
    const report = storage.loadFromText(system, files);
    if (modified) system.modified = true; // differs from what is stored in the browser
    refreshAll();
    let status = `${origin}: ${plural(report.studentsLoaded, 'student', 'students')}, ` +
      `${plural(report.coursesLoaded, 'course', 'courses')}, ${plural(report.enrollmentsLoaded, 'enrollment', 'enrollments')}.`;
    if (report.warnings.length) status += ` Problems found: ${report.warnings.length}.`;
    setStatus(status, report.warnings.length ? 'error' : 'ok');
    if (report.warnings.length)
      await showMessage({ title: 'Some data was skipped', text: storage.summarize(report, 15), kind: 'warning' });
  }

  function download(name, text) {
    const url = URL.createObjectURL(new Blob([text], { type: 'text/plain;charset=utf-8' }));
    const link = h('a', { href: url, download: name });
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }

  function exportFiles() {
    const files = storage.serialize(system);
    ['students', 'courses', 'enrollments'].forEach((name, i) =>
      setTimeout(() => download(`${name}.txt`, files[name]), i * 250));
    setStatus('Exported students.txt, courses.txt and enrollments.txt (check your Downloads folder).', 'ok');
  }

  async function importFiles(fileList) {
    const files = {};
    for (const file of fileList) {
      const name = file.name.toLowerCase();
      for (const kind of ['students', 'courses', 'enrollments']) {
        if (name.includes(kind) && name.endsWith('.txt')) files[kind] = await file.text();
      }
    }
    if (Object.keys(files).length === 0) {
      await showError('Nothing to import', 'Choose students.txt, courses.txt and/or enrollments.txt.');
      return;
    }
    const missing = ['students', 'courses', 'enrollments'].filter((k) => !(k in files)).map((k) => `${k}.txt`);
    const note = missing.length ? `\n\nNot selected (treated as empty): ${missing.join(', ')}.` : '';
    if (!(await confirmYesNo('Import files', `Replace all current data with the selected files?${note}`))) return;
    await applyFiles(files, { modified: true, origin: 'Imported' });
  }

  $('#btn-save').addEventListener('click', saveData);
  $('#btn-export').addEventListener('click', exportFiles);
  $('#btn-import').addEventListener('click', () => $('#file-input').click());
  $('#file-input').addEventListener('change', (event) => {
    const chosen = [...event.target.files];
    event.target.value = ''; // allows choosing the same files again
    if (chosen.length) importFiles(chosen);
  });
  $('#btn-sample').addEventListener('click', async () => {
    if (await confirmYesNo('Load sample data', 'Replace all current data with the demo data (6 students, 4 courses, 12 enrollments)?'))
      await applyFiles(storage.SAMPLE, { modified: true, origin: 'Sample data' });
  });
  $('#btn-about').addEventListener('click', () => $('#dlg-about').showModal());

  document.addEventListener('keydown', (event) => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') {
      event.preventDefault();
      saveData();
    }
  });

  // The browser can only offer its standard "leave this page?" prompt (no Yes/No/Cancel).
  window.addEventListener('beforeunload', (event) => {
    if (system.modified) { event.preventDefault(); event.returnValue = ''; }
  });

  // On phones the file actions fold away behind a "Data and files" toggle.
  const dataMenu = $('#datamenu');
  const narrow = window.matchMedia('(max-width: 900px)');
  const syncDataMenu = () => { dataMenu.open = !narrow.matches; };
  syncDataMenu();
  narrow.addEventListener('change', syncDataMenu);

  // ===========================================================================
  // Navigation tabs
  // ===========================================================================

  const TAB_NAMES = ['students', 'courses', 'enrollment'];

  function selectTab(name, moveFocus) {
    for (const tabName of TAB_NAMES) {
      const tab = $(`#tab-${tabName}`);
      const selected = tabName === name;
      tab.setAttribute('aria-selected', String(selected));
      tab.tabIndex = selected ? 0 : -1;
      $(`#panel-${tabName}`).hidden = !selected;
      if (selected && moveFocus) tab.focus();
    }
    if (name === 'enrollment') { animateSeats = true; refreshCourseStudents(); }
  }

  TAB_NAMES.forEach((name, index) => {
    const tab = $(`#tab-${name}`);
    tab.addEventListener('click', () => selectTab(name, false));
    tab.addEventListener('keydown', (event) => {
      const vertical = !narrow.matches;
      const step = { [vertical ? 'ArrowDown' : 'ArrowRight']: 1, [vertical ? 'ArrowUp' : 'ArrowLeft']: -1 }[event.key];
      let target = null;
      if (step) target = TAB_NAMES[(index + step + TAB_NAMES.length) % TAB_NAMES.length];
      else if (event.key === 'Home') target = TAB_NAMES[0];
      else if (event.key === 'End') target = TAB_NAMES[TAB_NAMES.length - 1];
      if (target) { event.preventDefault(); selectTab(target, true); }
    });
  });

  // ===========================================================================
  // Start-up: load what this browser saved, or the demo data on the first visit
  // ===========================================================================

  const saved = storage.readFromBrowser();
  if (saved) {
    applyFiles(saved, { modified: false, origin: 'Loaded' });
  } else {
    applyFiles(storage.SAMPLE, { modified: false, origin: 'Welcome! Demo data loaded' })
      .then(() => setStatus(`${lastAction} Press Save changes to keep your edits in this browser.`, 'info'));
  }
})();
