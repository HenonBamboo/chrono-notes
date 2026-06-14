#include "note_view.h"

#include <QTest>

class NoteViewTests : public QObject {
    Q_OBJECT

private slots:
    void archiveRowsStayInsideSingleSection();
    void searchRowsCarryHighlightAndSourceMeta();
    void searchRowsMarkFirstResultAsSectionStart();
};

void NoteViewTests::archiveRowsStayInsideSingleSection() {
    NoteStore store{};
    note_store_init(&store);
    note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-24", L"daily archived");

    NoteView::BuildRequest request;
    request.stage = NOTE_STAGE_MONTH;
    request.date_key = QStringLiteral("2026-05");

    const QVector<NoteView::Row> rows = NoteView::buildRows(&store, request);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).archive, true);
    QCOMPARE(rows.at(0).section, QStringLiteral("自动收纳"));
    QVERIFY2(rows.at(0).meta.contains(QStringLiteral("2026-05-24")), qPrintable(rows.at(0).meta));
}

void NoteViewTests::searchRowsCarryHighlightAndSourceMeta() {
    NoteStore store{};
    note_store_init(&store);
    note_store_add(&store, NOTE_STAGE_WEEK, L"2026-W21", L"alpha target");

    NoteView::BuildRequest request;
    request.stage = NOTE_STAGE_DAY;
    request.date_key = QStringLiteral("2026-05-24");
    request.search_query = QStringLiteral("target");

    const QVector<NoteView::Row> rows = NoteView::buildRows(&store, request);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).section, QStringLiteral("搜索结果"));
    QVERIFY2(rows.at(0).highlighted_text.contains(QStringLiteral("<mark>target</mark>")),
             qPrintable(rows.at(0).highlighted_text));
    QVERIFY2(rows.at(0).meta.contains(QStringLiteral("2026-W21")), qPrintable(rows.at(0).meta));
}

void NoteViewTests::searchRowsMarkFirstResultAsSectionStart() {
    NoteStore store{};
    note_store_init(&store);
    note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-24", L"target one");
    note_store_add(&store, NOTE_STAGE_WEEK, L"2026-W21", L"target two");

    NoteView::BuildRequest request;
    request.search_query = QStringLiteral("target");

    const QVector<NoteView::Row> rows = NoteView::buildRows(&store, request);

    QCOMPARE(rows.size(), 2);
    QVERIFY(!rows.at(0).section.isEmpty());
    QCOMPARE(rows.at(0).section, rows.at(1).section);
    QVERIFY(rows.at(0).section_first);
    QVERIFY(!rows.at(1).section_first);
}

QTEST_MAIN(NoteViewTests)

#include "note_view_tests.moc"
