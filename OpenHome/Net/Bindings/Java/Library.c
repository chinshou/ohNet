#include <jni.h>
#include "Library.h"
#include "OpenHome/Net/C/OhNet.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     openhome_net_core_Library
 * Method:    OhNetLibraryInitialise
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_org_openhome_net_core_Library_OhNetLibraryInitialise
  (JNIEnv *aEnv, jclass aClass, jlong aParams)
{
	OhNetHandleInitParams params = (OhNetHandleInitParams) (size_t)aParams;
	aEnv = aEnv;
	aClass = aClass;
	
    return (jlong) OhNetLibraryInitialise(params);
}

/*
 * Class:     org_openhome_net_core_Library
 * Method:    OhNetLibraryInitialiseMinimal
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_org_openhome_net_core_Library_OhNetLibraryInitialiseMinimal
  (JNIEnv *aEnv, jclass aClass, jlong aParams)
{
	OhNetHandleInitParams params = (OhNetHandleInitParams) (size_t)aParams;
	aEnv = aEnv;
	aClass = aClass;
	
	return (jlong) OhNetLibraryInitialiseMinimal(params);
}

/*
 * Class:     org_openhome_net_core_Library
 * Method:    OhNetLibraryStartCp
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_openhome_net_core_Library_OhNetLibraryStartCp
  (JNIEnv *aEnv, jclass aClass, jint aSubnet)
{
	TIpAddress subnet = (TIpAddress) aSubnet;
	aEnv = aEnv;
	aClass = aClass;
	
	return (jint) OhNetLibraryStartCp(subnet);
}

/*
 * Class:     org_openhome_net_core_Library
 * Method:    OhNetLibraryStartDv
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_openhome_net_core_Library_OhNetLibraryStartDv
  (JNIEnv *aEnv, jclass aClass)
{
	aEnv = aEnv;
	aClass = aClass;

	return (jint) OhNetLibraryStartDv();
}

/*
 * Class:     org_openhome_net_core_Library
 * Method:    OhNetLibraryStartCombined
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_openhome_net_core_Library_OhNetLibraryStartCombined
  (JNIEnv *aEnv, jclass aClass, jint aSubnet)
{
	TIpAddress subnet = (TIpAddress) aSubnet;
	aEnv = aEnv;
	aClass = aClass;
	
	return (jint) OhNetLibraryStartCombined(subnet);
}

/*
 * Class:     org_openhome_net_core_Library
 * Method:    OhNetLibraryClose
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_openhome_net_core_Library_OhNetLibraryClose
  (JNIEnv *aEnv, jclass aClass)
{
	aEnv = aEnv;
	aClass = aClass;

	OhNetLibraryClose();
}

/*
 * Class:     org_openhome_net_core_Library
 * Method:    OhNetSetCurrentSubnet
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_org_openhome_net_core_Library_OhNetSetCurrentSubnet
  (JNIEnv *aEnv, jclass aClass, jlong aSubnet)
{
	OhNetHandleNetworkAdapter subnet = (OhNetHandleNetworkAdapter) (size_t)aSubnet;
	aEnv = aEnv;
	aClass = aClass;
	
	OhNetSetCurrentSubnet(subnet);
}

/*
 * Class:     org_openhome_net_core_Library
 * Method:    OhNetCurrentSubnetAdapter
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_org_openhome_net_core_Library_OhNetCurrentSubnetAdapter
  (JNIEnv *aEnv, jclass aClass)
{
	aEnv = aEnv;
	aClass = aClass;

	return (jlong) (size_t)OhNetCurrentSubnetAdapter();
}

#ifdef __cplusplus
}
#endif
