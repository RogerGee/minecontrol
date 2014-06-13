// minecontrol-protocol.cpp
#include "minecontrol-protocol.h"
#include <cstring>
#include <rlibrary/rutility.h>
#include <openssl/rsa.h>
using namespace rtypes;
using namespace minecraft_controller;

/* the openssl library has global allocations that
   are outstanding; so as to not interfere with 
   memory leak checks, I invoke a non-thread-safe
   function to release that memory at application
   shutdown time; (there should be no other global
   destruction code that requires openssl) */
static class __openssl_initcleanup__
{
public:
    __openssl_initcleanup__()
    {
    }
    ~__openssl_initcleanup__()
    {
        CRYPTO_cleanup_all_ex_data();
    }
} __opensslinitcleanupobj__;

// minecraft_controller::crypt_session

/*static*/ const int crypt_session::_bits = 1024;
/*static*/ const int crypt_session::_bytes = 128;
crypt_session::crypt_session()
{
    // create a public-private key pair
    RSA* keypair;
    BIGNUM* e;
    if ((keypair = RSA_new()) == NULL)
        throw minecontrol_message_error();
    e = BN_new();
    BN_set_word(e,65537);
    if ( !RSA_generate_key_ex(keypair,_bits,e,NULL) )
        throw minecontrol_message_error();
    BN_free(e);
    _internal = keypair;
}
crypt_session::crypt_session(const generic_string& publicKey)
{
    // create a public key based on the input string
    RSA* pubkey;
    int lenN, lenE;
    const_stringstream ss(publicKey);
    unsigned char bufferN[1024];
    unsigned char bufferE[1024];
    ss.representation(hexadecimal);
    lenN = 0;
    while (ss.has_input() && ss.peek()!='|' && lenN<1024) {
        uint16 i;
        ss >> i;
        if ( !ss.get_input_success() )
            throw minecontrol_encrypt_error();
        bufferN[lenN++] = i&0xff;
    }
    if (ss.peek() != '|')
        throw minecontrol_encrypt_error();
    ss.get();
    lenE = 0;
    while (ss.has_input() && lenE<1024) {
        uint16 i;
        ss >> i;
        if ( !ss.get_input_success() )
            throw minecontrol_encrypt_error();
        bufferE[lenE++] = i&0xff;
    }
    if ((pubkey = RSA_new()) == NULL)
        throw minecontrol_message_error();
    pubkey->n = BN_bin2bn(bufferN,lenN,NULL);
    pubkey->e = BN_bin2bn(bufferE,lenE,NULL);
    _internal = pubkey;
}
crypt_session::~crypt_session()
{
    if (_internal != NULL)
        RSA_free(reinterpret_cast<RSA*>(_internal));
}
bool crypt_session::encrypt(const char* source,generic_string& result) const
{
    if (_internal != NULL) {
        int encryptlen;
        RSA* rsadata = reinterpret_cast<RSA*>(_internal);
        int length = strlen(source);
        unsigned char buffer[_bytes];
        // see if we have a public key
        if (rsadata->n==NULL || rsadata->e==NULL)
            return false;
        // encrypt data
        if ((encryptlen = RSA_public_encrypt(length,(unsigned char*)source,buffer,rsadata,RSA_PKCS1_PADDING)) == -1)
            return false;
        // encode data as hexadecimal text
        ref_stringstream ss(result);
        ss.clear();
        ss.representation(hexadecimal);
        ss.width(2);
        ss.fill('0');
        for (int i = 0;i < encryptlen;++i)
            ss << uint16(buffer[i]) << ' ';
        return true;
    }
    return false;
}
bool crypt_session::decrypt(const char* source,generic_string& result) const
{
    if (_internal != NULL) {
        int length;
        RSA* rsadata = reinterpret_cast<RSA*>(_internal);
        stringstream ss(source);
        int decryptLength;
        unsigned char bufferSource[_bytes];
        unsigned char bufferDestin[_bytes];
        // check to see if we have a private key
        if (rsadata->d == NULL)
            return false;
        // decode data from hexadecimal text
        ss.representation(hexadecimal);
        length = 0;
        while (ss.has_input() && length<_bytes) {
            uint16 i;
            ss >> i;
            if ( !ss.get_input_success() )
                return false;
            bufferSource[length++] = i&0xff;
        }
        if (length != _bytes)
            return false;
        // decrypt data
         if ((decryptLength = RSA_private_decrypt(length,bufferSource,bufferDestin,rsadata,RSA_PKCS1_PADDING)) == -1)
            return false;
        result.clear();
        for (int i = 0;i < decryptLength;++i)
            result.push_back(bufferDestin[i]);
        return true;
    }
    return false;
}
str crypt_session::get_public_key() const
{
    str result;
    if (_internal != NULL) {
        RSA* rsadata = reinterpret_cast<RSA*>(_internal);
        int lengthN = BN_num_bytes(rsadata->n);
        int lengthE = BN_num_bytes(rsadata->e);
        ref_stringstream ss(result);
        unsigned char* bufferN = new unsigned char[lengthN + lengthE];
        unsigned char* bufferE = bufferN+lengthN;
        // place public key into buffer
        BN_bn2bin(rsadata->n,bufferN);
        BN_bn2bin(rsadata->e,bufferE);
        /* get a text-representation of the public key in the following format:
            PUBLIC-MODULUS|PUBLIC-EXPONENT
             where PUBLIC-MODULUS and PUBLIC-EXPONENT are strings of hexadecimal
            digits that encode the binary number using little-endian representation */
        ss.representation(hexadecimal);
        ss.width(2);
        ss.fill('0');
        for (int i = 0;i < lengthN;++i)
            ss << uint16(bufferN[i]) << ' ';
        ss << "| ";
        for (int i = 0;i < lengthE;++i)
            ss << uint16(bufferE[i]) << ' ';
        delete[] bufferN;
    }
    return result;
}

// minecraft_controller::minecontrol_message

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
void minecontrol_message::add_field(const char* field,const char* value,const crypt_session* encrypt)
{
    _fields += field;
    _fields.push_back('\n'); // delimit the field name
    if (encrypt != NULL) {
        str result;
        if ( !encrypt->encrypt(value,result) )
            throw minecontrol_encrypt_error();
        _values += result;
    }
    else
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
str minecontrol_message::get_protocol_message() const
{
    str r;
    stringstream ss(r);
    ss << *this;
    return r;
}
void minecontrol_message::read_protocol_message(socket& input)
{
    socket_stream ss;
    ss.assign(input);
    ss >> *this;
}
void minecontrol_message::write_protocol_message(socket& output)
{
    socket_stream ss;
    ss.assign(output);
    ss << *this;
}
/*static*/ void minecontrol_message::_readProtocolLine(rstream& stream,str& line)
{
    // read a message line (separated by CRLF)
    str part;
    /* rstream objects ignore CR characters (\r) in getline operations; thus we can
       use getline to retrieve a line separated by LF; if CR is the last character in
       the line, then the message has been retrieved */
    line.clear();
    while (true) {
        stream.getline(part);
        if ( !stream.get_input_success() )
            break;
        line += part;
        if (part.length()>0 && part[part.length()-1]=='\r')
            break;
    }
    rutil_strip_whitespace_ref(line);
    stream.set_input_success(line.length() > 0);
}

rstream& minecraft_controller::operator >>(rstream& stream,minecontrol_message& msg)
{
    /* 'stream' can point to a number of different rstream derivations, each one performing
       a different input operation; it is imperative that the input operation not strip out 
       \r characters */
    str line;
    minecontrol_message::_readProtocolLine(stream,line);
    if (line == minecontrol_message::MINECONTROL_PROTO_HEADER) {
        msg._header = line;
        minecontrol_message::_readProtocolLine(stream,line);
        if ( stream.get_input_success() ) {
            msg._command = line;
            // read optional fields and values: each field should be
            //  of the form "field: value"
            msg.reset_fields();
            const_stringstream ss;
            ss.delimit_whitespace(false);
            ss.add_extra_delimiter(':');
            while (true) {
                str k, v;
                minecontrol_message::_readProtocolLine(stream,line);
                if ( !stream.get_input_success() )
                    break;
                // line is non-empty: try to parse field key and value
                ss.assign(line);
                // try to read in field name (key)
                ss >> k;
                if ( !ss.get_input_success() )
                    continue;
                // assume the rest of the string is the value
                ss.getline(v);
                if ( !ss.get_input_success() )
                    continue;
                // strip leading/trailing whitespace from key-value pair
                rutil_strip_whitespace_ref(k);
                rutil_strip_whitespace_ref(v);
                // add to minecontrol_message object payload
                msg.add_field(k.c_str(),v.c_str());
            }
            // normalize the command line and all fields to lower case
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
    /* 'stream' might point to a number of different rstream derivations, each one performing
       a different output operation with the message content; it is impertive that whatever
       implementation is chosen not strip out "\r" encoded strings */
    static const char* CRLF = "\r\n";
    str s;
    stream << msg._header << CRLF << msg._command << CRLF;
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
        stream << s << CRLF;
    }
    // be nice and reset the iterators
    msg._fieldKeys.set_input_iter(0);
    msg._fieldValues.set_input_iter(0);
    // add final newline and flush stream
    return stream << CRLF << flush;
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
void minecontrol_message_buffer::enqueue_field_name(const char* fieldName,const crypt_session* encrypt)
{
    _fields.push( field_item(fieldName,encrypt) );
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
        const field_item field = _fields.pop();
        while ( !_bufOut.is_empty() ) {
            char c = _bufOut.pop();
            if (c == '\n')
                break;
            value.push_back(c);
        }
        _message.add_field(field.field.c_str(),value.c_str(),field.encrypt);
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
