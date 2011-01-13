#ifndef HEADER_SERVICEC
#define HEADER_SERVICEC

#include <OsTypes.h>
#include <C/Zapp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup Service
 * @ingroup Core
 * @{
 */

/**
 * Opaque handle to a service
 */
typedef THandle HandleService;

/**
 * @addtogroup Parameter
 * @ingroup Service
 * @{
 */
/**
 * Opaque handle to a parameter.
 * Each action owns 0..n of these.
 */
typedef THandle ServiceParameter;

const int32_t kServiceParameterIntDefaultMin  = INT32_MIN;
const int32_t kServiceParameterIntDefaultMax  = INT32_MAX;
const int32_t kServiceParameterIntDefaultStep = 1;
const uint32_t kServiceParameterUintDefaultMin  = 0;
const uint32_t kServiceParameterUintDefaultMax  = UINT32_MAX;
const uint32_t kServiceParameterUintDefaultStep = 1;

/**
 * Create an integer parameter
 *
 * Note that no destructor exists.  Parameters should be added to an action or property which takes ownership of them.
 *
 * @param[in] aName      Name to assign to this parameter
 * @param[in] aMinValue  Minimum allowed value.  Use kServiceParameterIntDefaultMin by default.
 * @param[in] aMaxValue  Maximum allowed value.  Use kServiceParameterIntDefaultMax by default.
 * @param[in] aStep      Non-negative increment between allowed values.  Use kServiceParameterIntDefaultStep by default
 *
 * @return   Handle to the newly created service
 */
DllExport ServiceParameter ServiceParameterCreateInt(const char* aName, int32_t aMinValue, int32_t aMaxValue, int32_t aStep);

/**
 * Create an unsigned integer parameter
 *
 * Note that no destructor exists.  Parameters should be added to an action or property which takes ownership of them.
 *
 * @param[in] aName      Name to assign to this parameter
 * @param[in] aMinValue  Minimum allowed value.  Use kServiceParameterUintDefaultMin by default.
 * @param[in] aMaxValue  Maximum allowed value.  Use kServiceParameterUintDefaultMax by default.
 * @param[in] aStep      Non-negative increment between allowed values.  Use kServiceParameterUintDefaultStep by default
 *
 * @return   Handle to the newly created service
 */
DllExport ServiceParameter ServiceParameterCreateUint(const char* aName, uint32_t aMinValue, uint32_t aMaxValue, uint32_t aStep);

/**
 * Create a boolean parameter
 *
 * Note that no destructor exists.  Parameters should be added to an action or property which takes ownership of them.
 *
 * @param[in] aName      Name to assign to this parameter
 *
 * @return   Handle to the newly created service
 */
DllExport ServiceParameter ServiceParameterCreateBool(const char* aName);

/**
 * Create a string parameter
 *
 * Note that no destructor exists.  Parameters should be added to an action or property which takes ownership of them.
 *
 * @param[in] aName           Name to assign to this parameter
 * @param[in] aAllowedValues  Array of strings limiting the permitted values of this parameter.  May be NULL.
 * @param[in] aCount          Number of allowed values.  Must be 0 if aAllowedValues is NULL.
 *
 * @return   Handle to the newly created service
 */
DllExport ServiceParameter ServiceParameterCreateString(const char* aName, char** aAllowedValues, uint32_t aCount);

/**
 * Create a binary parameter
 *
 * Note that no destructor exists.  Parameters should be added to an action or property which takes ownership of them.
 *
 * @param[in] aName      Name to assign to this parameter
 *
 * @return   Handle to the newly created service
 */
DllExport ServiceParameter ServiceParameterCreateBinary(const char* aName);
/**
 * @addtogroup Property
 * @ingroup Service
 * @{
 */
/**
 * Opaque handle to a property
 */
typedef THandle ServiceProperty;
/* @} */
/**
 * @addtogroup Parameter
 * @ingroup Service
 * @{
 */
/**
 * Create a related parameter
 *
 * A related parameter reads its allowed values from an existing ServiceProperty.  It is normally useful
 * to users of the device stack.  Any type of property can be used.  However, since only int, uint and
 * string types can have bounds applied, it only makes sense to use these types of property.
 * Note that no destructor exists.  Parameters should be added to an action or property which takes ownership of them.
 *
 * @param[in] aName      Name to assign to this parameter
 * @param[in] aProperty  Property to supply this parameter's value bounds
 *
 * @return   Handle to the newly created service
 */
DllExport ServiceParameter ServiceParameterCreateRelated(const char* aName, ServiceProperty aProperty);
/* @} */

/**
 * @addtogroup Property
 * @ingroup Service
 * @{
 */
/**
 * Opaque handle to a property.  One property will exist for each property (state variable) of a service.
 */
typedef THandle ServiceProperty;

/**
 * Create an integer property suitable for use by a client of the control point stack
 *
 * Note that no destructor exists.  Properties must be added to proxies or providers, which take ownership of them
 *
 * @param[in] aName      Name to assign to the property
 * @param[in] aCallback  Function to run when the value of the property changes
 * @param[in] aPtr       Data to pass to the callback.  May be NULL.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateIntCp(const char* aName, ZappCallback aCallback, void* aPtr);

/**
 * Create an integer property suitable for use by a client of the device stack
 *
 * @param[in] aParameter  Returned by ServiceParameterCreateInt.  Ownership passed to this property.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateIntDv(ServiceParameter aParameter);

/**
 * Create an integer property suitable for use by a client of the control point stack
 *
 * Note that no destructor exists.  Properties must be added to proxies or providers, which take ownership of them
 *
 * @param[in] aName      Name to assign to the property
 * @param[in] aCallback  Function to run when the value of the property changes
 * @param[in] aPtr       Data to pass to the callback.  May be NULL.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateUintCp(const char* aName, ZappCallback aCallback, void* aPtr);

/**
 * Create an unsigned integer property suitable for use by a client of the device stack
 *
 * @param[in] aParameter  Returned by ServiceParameterCreateUint.  Ownership passed to this property.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateUintDv(ServiceParameter aParameter);

/**
 * Create a boolean property suitable for use by a control point
 *
 * Note that no destructor exists.  Properties must be added to proxies or providers, which take ownership of them
 *
 * @param[in] aName      Name to assign to the property
 * @param[in] aCallback  Function to run when the value of the property changes
 * @param[in] aPtr       Data to pass to the callback.  May be NULL.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateBoolCp(const char* aName, ZappCallback aCallback, void* aPtr);

/**
 * Create a boolean property suitable for use by a client of the device stack
 *
 * @param[in] aParameter  Returned by ServiceParameterCreateBool.  Ownership passed to this property.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateBoolDv(ServiceParameter aParameter);

/**
 * Create an integer property suitable for use by a client of the control point stack
 *
 * Note that no destructor exists.  Properties must be added to proxies or providers, which take ownership of them
 *
 * @param[in] aName      Name to assign to the property
 * @param[in] aCallback  Function to run when the value of the property changes
 * @param[in] aPtr       Data to pass to the callback.  May be NULL.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateStringCp(const char* aName, ZappCallback aCallback, void* aPtr);

/**
 * Create a string property suitable for use by a client of the device stack
 *
 * @param[in] aParameter  Returned by ServiceParameterCreateString.  Ownership passed to this property.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateStringDv(ServiceParameter aParameter);

/**
 * Create an integer property suitable for use by a client of the control point stack
 *
 * Note that no destructor exists.  Properties must be added to proxies or providers, which take ownership of them
 *
 * @param[in] aName      Name to assign to the property
 * @param[in] aCallback  Function to run when the value of the property changes
 * @param[in] aPtr       Data to pass to the callback.  May be NULL.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateBinaryCp(const char* aName, ZappCallback aCallback, void* aPtr);

/**
 * Create a binary property suitable for use by a client of the device stack
 *
 * @param[in] aParameter  Returned by ServiceParameterCreateBinary.  Ownership passed to this property.
 *
 * @return   Handle to the newly created property
 */
DllExport ServiceProperty ServicePropertyCreateBinaryDv(ServiceParameter aParameter);

/**
 * Read the current value of an integer property
 *
 * @param[in] aProperty  Returned by ServicePropertyCreateInt[Cp|Dv]
 *
 * @return   Current value of the property
 */
DllExport int32_t ServicePropertyValueInt(ServiceProperty aProperty);

/**
 * Read the current value of an unsigned integer property
 *
 * @param[in] aProperty  Returned by ServicePropertyCreateUint[Cp|Dv]
 *
 * @return   Current value of the property
 */
DllExport uint32_t ServicePropertyValueUint(ServiceProperty aProperty);

/**
 * Read the current value of a boolean property
 *
 * @param[in] aProperty  Returned by ServicePropertyCreateBool[Cp|Dv]
 *
 * @return   Current value of the property.  0 for false; non-zero for true
 */
DllExport uint32_t ServicePropertyValueBool(ServiceProperty aProperty);

/**
 * Read the current value of a string property
 *
 * @param[in] aProperty  Returned by ServicePropertyCreateString[Cp|Dv]
 *
 * @return   Copy of the current value of the property.  Ownership is passed to the caller.
 *           Use ZappFree to later free it
 */
DllExport const char* ServicePropertyValueString(ServiceProperty aProperty);

/**
 * Read the current value of a binary property
 *
 * @param[in]  aProperty  Returned by ServicePropertyCreateBinary[Cp|Dv]
 * @param[out] aData      Copy of the current value of the property.  Ownership is passed to the caller.
 *                        Use ZappFree to later free it
 * @param[out] aLen       Number of bytes of data returned
 */
DllExport void ServicePropertyGetValueBinary(ServiceProperty aProperty, const uint8_t** aData, uint32_t* aLen);

/* @} */

/**
 * @addtogroup Action
 * @ingroup Service
 * @{
 */
/**
 * Opaque handle to an action
 *
 * Each service has 0..n of these.  Each action has 0..n input parameters and
 * 0..m output parameters.  Each parameter must be either input or output.
 */
typedef THandle ServiceAction;

/**
 * Create an action
 *
 * @param[in] aName  Name of the action
 *
 * @return   Handle to the newly created action
 */
DllExport ServiceAction ServiceActionCreate(const char* aName);

/**
 * Destroy an action
 *
 * @param[in] aAction     Returned by ServiceActionCreate
 */
DllExport void ServiceActionDestroy(ServiceAction aAction);

/**
 * Add an input parameter to an action
 *
 * @param[in] aAction     Returned by ServiceActionCreate
 * @param[in] aParameter  Input parameter to be added.  Ownership passes to the action
 */
DllExport void ServiceActionAddInputParameter(ServiceAction aAction, ServiceParameter aParameter);

/**
 * Add an output parameter to an action
 *
 * @param[in] aAction     Returned by ServiceActionCreate
 * @param[in] aParameter  Input parameter to be added.  Ownership passes to the action
 */
DllExport void ServiceActionAddOutputParameter(ServiceAction aAction, ServiceParameter aParameter);

/**
 * Query the action name
 *
 * @param[in] aAction     Returned by ServiceActionCreate
 *
 * @return   The action name
 */
DllExport const char* ServiceActionName(ServiceAction aAction);

/* @} */

/* @} */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HEADER_SERVICEC