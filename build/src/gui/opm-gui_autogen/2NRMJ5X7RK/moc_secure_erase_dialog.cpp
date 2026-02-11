/****************************************************************************
** Meta object code from reading C++ file 'secure_erase_dialog.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../src/gui/dialogs/secure_erase_dialog.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'secure_erase_dialog.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_opm__gui__SecureEraseDialog_t {
    QByteArrayData data[8];
    char stringdata0[102];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_opm__gui__SecureEraseDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_opm__gui__SecureEraseDialog_t qt_meta_stringdata_opm__gui__SecureEraseDialog = {
    {
QT_MOC_LITERAL(0, 0, 27), // "opm::gui::SecureEraseDialog"
QT_MOC_LITERAL(1, 28, 15), // "onMethodChanged"
QT_MOC_LITERAL(2, 44, 0), // ""
QT_MOC_LITERAL(3, 45, 5), // "index"
QT_MOC_LITERAL(4, 51, 15), // "onVerifyChanged"
QT_MOC_LITERAL(5, 67, 5), // "state"
QT_MOC_LITERAL(6, 73, 14), // "onStartClicked"
QT_MOC_LITERAL(7, 88, 13) // "validateInput"

    },
    "opm::gui::SecureEraseDialog\0onMethodChanged\0"
    "\0index\0onVerifyChanged\0state\0"
    "onStartClicked\0validateInput"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_opm__gui__SecureEraseDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x08 /* Private */,
       4,    1,   37,    2, 0x08 /* Private */,
       6,    0,   40,    2, 0x08 /* Private */,
       7,    0,   41,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void,
    QMetaType::Bool,

       0        // eod
};

void opm::gui::SecureEraseDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SecureEraseDialog *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onMethodChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->onVerifyChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->onStartClicked(); break;
        case 3: { bool _r = _t->validateInput();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject opm::gui::SecureEraseDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_opm__gui__SecureEraseDialog.data,
    qt_meta_data_opm__gui__SecureEraseDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *opm::gui::SecureEraseDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *opm::gui::SecureEraseDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_opm__gui__SecureEraseDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int opm::gui::SecureEraseDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
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
QT_WARNING_POP
QT_END_MOC_NAMESPACE
