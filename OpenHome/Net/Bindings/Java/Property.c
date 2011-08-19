#include <jni.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include "Property.h"
#include "PropertyCallback.h"
#include "JniCallbackList.h"
#include "OpenHome/Net/C/OhNet.h"
#include "OpenHome/Net/C/Service.h"

#ifdef __cplusplus
extern "C" {
#endif

void STDCALL ChangeCallback(void* aPtr)
{
	JniObjRef* ref = (JniObjRef*) aPtr;
	JNIEnv *env;
	jclass cls;
	jmethodID mid;
	jint ret;
	jint attached;

	attached = (*(ref->vm))->GetEnv(ref->vm, (void **)&env, JNI_VERSION_1_4);
	if (attached < 0)
	{
#ifdef __ANDROID__
		ret = (*(ref->vm))->AttachCurrentThread(ref->vm, &env, NULL);
#else
		ret = (*(ref->vm))->AttachCurrentThread(ref->vm, (void **)&env, NULL);
#endif
		if (ret < 0)
		{
			printf("PropertyJNI: Unable to attach thread to JVM.\n");
			fflush(stdout);
			return;
		}
	}
	cls = (*env)->GetObjectClass(env, ref->callbackObj);
	mid = (*env)->GetMethodID(env, cls, "notifyChange", "()V");
	if (mid == 0) {
		printf("PropertyJNI: Method ID notifyChange() not found.\n");
		return;
	}
	(*env)->CallVoidMethod(env, ref->callbackObj, mid);
	
	if (attached < 0)
	{
		(*(ref->vm))->DetachCurrentThread(ref->vm);
	}
}

void STDCALL InitialiseReferences(JNIEnv *aEnv, jobject aObject, JniObjRef **aRef)
{
	jint ret;

	*aRef = (JniObjRef*) malloc(sizeof(JniObjRef));

	ret = (*aEnv)->GetJavaVM(aEnv, &(*aRef)->vm);
	if (ret < 0) {
		printf("PropertyJNI: Unable to get reference to the current Java VM.\n");
		fflush(stdout);
	}
	(*aRef)->callbackObj = (*aEnv)->NewGlobalRef(aEnv, aObject);
	if ((*aRef)->callbackObj == NULL) {
		printf("PropertyJNI: Callback object not stored.\n");
		fflush(stdout);
	}
}

/*
 * Class:     org_openhome_net_core_Property
 * Method:    ServicePropertyDestroy
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_org_openhome_net_core_Property_ServicePropertyDestroy
  (JNIEnv *aEnv, jclass aClass, jlong aProperty, jlong aCallback)
{
	ServiceProperty property = (ServiceProperty) (size_t)aProperty;
	JniObjRef *ref = (JniObjRef*) (size_t)aCallback;
	aClass = aClass;
	
	(*aEnv)->DeleteGlobalRef(aEnv, ref->callbackObj);
	free(ref);
	ServicePropertyDestroy(property);
}

#ifdef __cplusplus
}
#endif
