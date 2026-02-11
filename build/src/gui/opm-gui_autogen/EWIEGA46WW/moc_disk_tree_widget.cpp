/****************************************************************************
** Meta object code from reading C++ file 'disk_tree_widget.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../src/gui/disk_tree_widget.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'disk_tree_widget.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_opm__gui__DiskTreeWidget_t {
    QByteArrayData data[10];
    char stringdata0[135];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_opm__gui__DiskTreeWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_opm__gui__DiskTreeWidget_t qt_meta_stringdata_opm__gui__DiskTreeWidget = {
    {
QT_MOC_LITERAL(0, 0, 24), // "opm::gui::DiskTreeWidget"
QT_MOC_LITERAL(1, 25, 12), // "diskSelected"
QT_MOC_LITERAL(2, 38, 0), // ""
QT_MOC_LITERAL(3, 39, 5), // "index"
QT_MOC_LITERAL(4, 45, 17), // "diskDoubleClicked"
QT_MOC_LITERAL(5, 63, 22), // "onItemSelectionChanged"
QT_MOC_LITERAL(6, 86, 19), // "onItemDoubleClicked"
QT_MOC_LITERAL(7, 106, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(8, 123, 4), // "item"
QT_MOC_LITERAL(9, 128, 6) // "column"

    },
    "opm::gui::DiskTreeWidget\0diskSelected\0"
    "\0index\0diskDoubleClicked\0"
    "onItemSelectionChanged\0onItemDoubleClicked\0"
    "QTreeWidgetItem*\0item\0column"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_opm__gui__DiskTreeWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x06 /* Public */,
       4,    1,   37,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    0,   40,    2, 0x08 /* Private */,
       6,    2,   41,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 7, QMetaType::Int,    8,    9,

       0        // eod
};

void opm::gui::DiskTreeWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DiskTreeWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->diskSelected((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->diskDoubleClicked((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->onItemSelectionChanged(); break;
        case 3: _t->onItemDoubleClicked((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (DiskTreeWidget::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&DiskTreeWidget::diskSelected)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (DiskTreeWidget::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&DiskTreeWidget::diskDoubleClicked)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject opm::gui::DiskTreeWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QTreeWidget::staticMetaObject>(),
    qt_meta_stringdata_opm__gui__DiskTreeWidget.data,
    qt_meta_data_opm__gui__DiskTreeWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *opm::gui::DiskTreeWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *opm::gui::DiskTreeWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_opm__gui__DiskTreeWidget.stringdata0))
        return static_cast<void*>(this);
    return QTreeWidget::qt_metacast(_clname);
}

int opm::gui::DiskTreeWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QTreeWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void opm::gui::DiskTreeWidget::diskSelected(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void opm::gui::DiskTreeWidget::diskDoubleClicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
