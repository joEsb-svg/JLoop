#define JUCE_CORE_INCLUDE_JNI_HELPERS 1
#include "OpenWith.h"

namespace OpenWith
{
    juce::String takePendingUri()
    {
       #if JUCE_ANDROID
        using namespace juce;
        auto* env = getEnv();
        if (env == nullptr) return {};
        auto activity = getMainActivity();
        if (activity.get() == nullptr) return {};

        LocalRef<jclass> activityClass (env->GetObjectClass (activity.get()));
        const auto getIntent = env->GetMethodID (activityClass.get(), "getIntent", "()Landroid/content/Intent;");
        const auto setIntent = env->GetMethodID (activityClass.get(), "setIntent", "(Landroid/content/Intent;)V");
        if (getIntent == nullptr || setIntent == nullptr) { env->ExceptionClear(); return {}; }

        LocalRef<jobject> intent (env->CallObjectMethod (activity.get(), getIntent));
        if (intent.get() == nullptr) return {};

        LocalRef<jclass> intentClass (env->GetObjectClass (intent.get()));
        const auto getAction = env->GetMethodID (intentClass.get(), "getAction", "()Ljava/lang/String;");
        const auto getData   = env->GetMethodID (intentClass.get(), "getData", "()Landroid/net/Uri;");
        const auto ctor      = env->GetMethodID (intentClass.get(), "<init>", "()V");
        if (getAction == nullptr || getData == nullptr || ctor == nullptr) { env->ExceptionClear(); return {}; }

        LocalRef<jstring> action ((jstring) env->CallObjectMethod (intent.get(), getAction));
        if (action.get() == nullptr || juceString (action.get()) != "android.intent.action.VIEW")
            return {};

        LocalRef<jobject> uri (env->CallObjectMethod (intent.get(), getData));
        String result;
        if (uri.get() != nullptr)
        {
            LocalRef<jclass> uriClass (env->GetObjectClass (uri.get()));
            const auto toString = env->GetMethodID (uriClass.get(), "toString", "()Ljava/lang/String;");
            LocalRef<jstring> s ((jstring) env->CallObjectMethod (uri.get(), toString));
            if (s.get() != nullptr) result = juceString (s.get());
        }

        // consumato: la prossima volta non lo riapre
        LocalRef<jobject> blank (env->NewObject (intentClass.get(), ctor));
        env->CallVoidMethod (activity.get(), setIntent, blank.get());
        if (env->ExceptionCheck()) env->ExceptionClear();
        return result;
       #else
        return {};
       #endif
    }
}
