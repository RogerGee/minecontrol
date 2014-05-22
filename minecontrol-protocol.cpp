// minecontrol-protocol.cpp
#include "minecontrol-protocol.h"
#include "rlibrary/rutility.h"
using namespace rtypes;
using namespace minecraft_controller;

/*static*/ const char* const minecontrol_message::MINECONTROL_PROTO_HEADER = "MINECONTROL-PROTOCOL";
minecontrol_message::minecontrol_message()
    : _state(false)
{
    // set up const rstreams to handle fields and values
    _fieldKeys.delimit_whitespace(false);
    _fieldKeys.add_extra_delimiter('\n');
    _fieldValues.delimit_whitespace(false);
    _fieldValues.add_extra_delimiter('\n');
    _fieldKeys.assign(_fields);
    _fieldValues.assign(_values);
}
minecontrol_message::minecontrol_message(const char* command)
    : _state(true)
{
    _header = MINECONTROL_PROTO_HEADER;
    _command = command;
    // set up const rstreams to handle fields and values
    _fieldKeys.delimit_whitespace(false);
    _fieldKeys.add_extra_delimiter('\n');
    _fieldValues.delimit_whitespace(false);
    _fieldValues.add_extra_delimiter('\n');
    _fieldKeys.assign(_fields);
    _fieldValues.assign(_values);
}
void minecontrol_message::assign_command(const char* command)
{
    _header = MINECONTROL_PROTO_HEADER;
    _command = command;
}
void minecontrol_message::add_field(const char* field,const char* value)
{
    _fields += field;
    _fields.push_back('\n'); // delimit the field name
    _values += value;
    _values.push_back('\n'); // delimit the value
}
void minecontrol_message::reset_fields()
{
    _fields.clear();
    _values.clear();
    _fieldKeys.clear();
    _fieldValues.clear();
}

rstream& minecraft_controller::operator >>(rstream& stream,minecontrol_message& msg)
{
    // read message according to minecontrol protocol
    string line;
    stream.getline(line);
    if (line == minecontrol_message::MINECONTROL_PROTO_HEADER) {
        msg._header = line;
        stream.getline(line);
        if ( stream.get_input_success() ) {
            msg._command = line;
            // read optional fields and values: each field should be
            //  of the form "field: value"
            msg.reset_fields();
            const_rstringstream ss;
            ss.delimit_whitespace(false);
            ss.add_extra_delimiter(':');
            while (true) {
                str k, v;
                stream.getline(line);
                if (!stream.get_input_success() || line.length()==0)
                    break;
                ss.assign(line);
                ss >> k;
                ss.getline(v);
                if ( !ss.get_input_success() )
                    break;
                rutil_strip_whitespace_ref(k);
                rutil_strip_whitespace_ref(v);
                msg.add_field(k.c_str(),v.c_str());
            }
            rutil_to_lower_ref(msg._command);
            rutil_to_lower_ref(msg._fields);
            // indicate success
            msg._state = true;
            return stream;
        }
    }
    msg._state = false;
    return stream;
}

rstream& minecraft_controller::operator <<(rstream& stream,const minecontrol_message& msg)
{
    str s;
    stream << msg._header << newline << msg._command << newline;
    // ensure iterator is at beginning position
    msg._fieldKeys.set_input_iter(0);
    msg._fieldValues.set_input_iter(0);
    while (true) {
        msg._fieldKeys >> s;
        if ( !msg._fieldKeys.get_input_success() )
            break;
        stream << s << ": ";
        msg._fieldValues >> s;
        if ( !msg._fieldValues.get_input_success() )
            break;
        stream << s << newline;
    }
    // be nice and reset the iterators
    msg._fieldKeys.set_input_iter(0);
    msg._fieldValues.set_input_iter(0);
    // add final newline and flush stream
    return stream << endline;
}

minecontrol_message_buffer::minecontrol_message_buffer()
    : _repeatField(NULL)
{
}
void minecontrol_message_buffer::begin(const char* command)
{
    _message.assign_command(command);
    _message.reset_fields();
    _fields.clear();
    _repeatField = NULL;
}
void minecontrol_message_buffer::enqueue_field_name(const char* fieldName)
{
    _fields.push(fieldName);
}
void minecontrol_message_buffer::repeat_field(const char* fieldName)
{
    _repeatField = fieldName;
}
uint32 minecontrol_message_buffer::count_fields() const
{
    uint32 cnt = 0;
    if (_bufOut.size() > 0) {
        uint32 i;
        bool state = true;
        const char* data = &_bufOut.peek();
        while (i < _bufOut.size()) {
            if (state) {
                ++cnt;
                state = false;
            }
            if (data[i] == '\n')
                state = true;
            ++i;
        }
    }
    return cnt;
}
void minecontrol_message_buffer::_outDevice()
{
    while ( !_fields.is_empty() ) {
        str value;
        const string field = _fields.pop();
        while ( !_bufOut.is_empty() ) {
            char c = _bufOut.pop();
            if (c == '\n')
                break;
            value.push_back(c);
        }
        _message.add_field(field.c_str(),value.c_str());
    }
    if (_repeatField != NULL) {
        while ( !_bufOut.is_empty() ) {
            str value;
            while ( !_bufOut.is_empty() ) {
                char c = _bufOut.pop();
                if (c == '\n')
                    break;
                value.push_back(c);
            }
            _message.add_field(_repeatField,value.c_str());
        }
    }
}
