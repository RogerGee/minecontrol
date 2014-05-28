// minecraft-server-properties.tcc - out-of-line implementation

// minecraft_controller::minecraft_server_property
template<typename T>
minecraft_controller::minecraft_server_property<T>::minecraft_server_property()
{
    _isNull = true;
    _isDefault = true;
}
template<typename T>
minecraft_controller::minecraft_server_property<T>::minecraft_server_property(const T& defaultValue)
    : _value(defaultValue)
{
    _isNull = false;
    _isDefault = true;
}
template<typename T>
minecraft_controller::minecraft_server_property<T>::~minecraft_server_property()
{
}
template<typename T>
void minecraft_controller::minecraft_server_property<T>::put(rtypes::rstream& stream) const
{
    stream << _getKeyName() << '=';
    if (!_isNull)
        _putValue(stream);
}
template<typename T>
bool minecraft_controller::minecraft_server_property<T>::set_value(const rtypes::str& value)
{
    rtypes::const_stringstream ss(value);
    if ( _readValue(ss) )
    {
        // since the user intentionally is setting the value (and it was successful),
        // then it shouldn't be left null
        _isNull = false;
        // flag the value as being user-supplied; this is important for the server manager
        // when applying default properties (so that it doesn't override a user's property)
        _isDefault = false;
        return true;
    }
    return false;
}
template<typename T>
bool minecraft_controller::minecraft_server_property<T>::_readValue(rtypes::rstream& stream)
{
    stream >> _value;
    return stream.get_input_success();
}
template<typename T>
void minecraft_controller::minecraft_server_property<T>::_putValue(rtypes::rstream& stream) const
{
    stream << _value;
}

// minecraft_controller::minecraft_property_generic_list

template<typename T>
minecraft_controller::minecraft_property_generic_list<T>::~minecraft_property_generic_list()
{
    // assume each pointer points to dynamically allocated memory
    for (rtypes::size_type i = 0;i<_list.size();i++)
        delete reinterpret_cast<minecraft_controller::minecraft_server_property<T>*>(_list[i]);
}
template<typename T>
void* minecraft_controller::minecraft_property_generic_list<T>::_lookup(const char* pname)
{
    for(rtypes::size_type i = 0;i<_list.size();i++)
        if (::strcmp(reinterpret_cast<minecraft_controller::minecraft_server_property<T>*>(_list[i])->get_key(),pname) == 0)
            return _list[i];
    return NULL;
}
template<typename T>
const void* minecraft_controller::minecraft_property_generic_list<T>::_lookup(const char* pname) const
{
    for(rtypes::size_type i = 0;i<_list.size();i++)
        if (::strcmp(reinterpret_cast<minecraft_server_property<T>*>(_list[i])->get_key(),pname) == 0)
            return _list[i];
    return NULL;
}
