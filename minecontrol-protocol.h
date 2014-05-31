// minecontrol-protocol.h
#ifndef MINECONTROL_PROTOCOL_H
#define MINECONTROL_PROTOCOL_H
#include <rlibrary/rqueue.h>
#include <rlibrary/rstringstream.h> // gets rstream

namespace minecraft_controller
{
    class minecontrol_message_error { };
    class minecontrol_encrypt_error { }; // these should be handled

    /* crypt_session
     *  represents an RSA public-key encryptor/decryptor
     */
    class crypt_session
    {
    public:
        crypt_session();
        crypt_session(const rtypes::generic_string& publicKey);
        ~crypt_session();

        bool encrypt(const char* source,rtypes::generic_string& result) const;
        bool decrypt(const char* source,rtypes::generic_string& result) const;
        rtypes::string get_public_key() const;
    private:
        static const int _bits;
        static const int _bytes;
        void* _internal;
    };

    /* minecontrol_message:
     *  represents a message used in implementing the simple
     * minecontrol communications protocol; every command name
     * and field, value pair are normalized to lower case
     */
    class minecontrol_message
    {
        friend rtypes::rstream& operator >>(rtypes::rstream&,minecontrol_message&);
        friend rtypes::rstream& operator <<(rtypes::rstream&,const minecontrol_message&);
    public:
        minecontrol_message();
        minecontrol_message(const char* command);

        void assign_command(const char* command);
        void add_field(const char* field,const char* value,const crypt_session* pencrypt = NULL);
        void reset_fields();

        bool good() const
        { return _state; }
        bool is_command(const char* command) const
        { return _command == command; }

        const char* get_header() const
        { return _header.c_str(); }
        const char* get_command() const
        { return _command.c_str(); }
        const char* get_fields() const
        { return _fields.c_str(); }
        const char* get_values() const
        { return _values.c_str(); }
        rtypes::rstream& get_field_key_stream() const
        { return _fieldKeys; }
        rtypes::rstream& get_field_value_stream() const
        { return _fieldValues; }
    private:
        static const char* const MINECONTROL_PROTO_HEADER;

        bool _state;
        rtypes::str _header;
        rtypes::str _command;
        rtypes::str _fields, _values;
        mutable rtypes::const_stringstream _fieldKeys;
        mutable rtypes::const_stringstream _fieldValues;
    };

    rtypes::rstream& operator >>(rtypes::rstream&,minecontrol_message&);
    rtypes::rstream& operator <<(rtypes::rstream&,const minecontrol_message&);

    /* minecontrol_message_buffer
     *  simplifies the creation of minecontrol messages by providing
     * a local buffer for field values; maintains a queue of desired
     * fields and expects their values on the stream; each field is 
     * delimited by a newline
     */
    class minecontrol_message_buffer : public rtypes::rstream
    {
    public:
        minecontrol_message_buffer();

        void begin(const char* command);

        // add a field name to be expected; these are queued
        void enqueue_field_name(const char* fieldName,const crypt_session* encrypt = NULL);
        void repeat_field(const char* fieldName);

        // retrieve the internal message that is being compiled
        const minecontrol_message& get_message() const
        { return _message; }

        rtypes::uint32 count_fields() const;
    private:
        struct field_item
        {
            field_item() {}
            field_item(const char* fieldName,const crypt_session* pencrypt)
                : field(fieldName), encrypt(pencrypt) {}

            rtypes::string field;
            const crypt_session* encrypt;
        };

        minecontrol_message _message;
        rtypes::queue<field_item> _fields;
        const char* _repeatField;

        virtual bool _inDevice() const
        { return false; } // no input expected from this stream buffer
        virtual void _outDevice();
    };
}

#endif
