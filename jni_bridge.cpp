#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_pes17_recomp_MainActivity_getNativeMessage(JNIEnv* env, jobject /* this */) {
    std::string hello = "O Cérebro C++ do PES17 está rodando!";
    return env->NewStringUTF(hello.c_str());
}
