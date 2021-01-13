// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/android/xwalk_settings.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/supports_user_data.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "xwalk/runtime/android/core_refactor/xwalk_refactor_native_jni/XWalkSettings_jni.h"
#include "xwalk/runtime/browser/android/renderer_host/xwalk_render_view_host_ext.h"
#include "xwalk/runtime/browser/android/xwalk_content.h"
#include "xwalk/runtime/browser/xwalk_browser_context.h"
#include "xwalk/runtime/browser/xwalk_content_browser_client.h"
#include "xwalk/runtime/common/xwalk_content_client.h"
#include "xwalk/runtime/common/xwalk_switches.h"

using base::android::CheckException;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::ScopedJavaLocalRef;
using base::android::JavaParamRef;
using content::WebPreferences;

namespace xwalk {

namespace {

const void* const kXWalkSettingsUserDataKey = &kXWalkSettingsUserDataKey;

void PopulateFixedWebPreferences(content::WebPreferences* webPrefs) {
  webPrefs->shrinks_standalone_images_to_fit = false;
  webPrefs->should_clear_document_background = true;
  webPrefs->viewport_meta_enabled = true;
  webPrefs->picture_in_picture_enabled = false;
}

jfieldID GetFieldID(JNIEnv* env,
                    const base::android::JavaRef<jclass>& clazz,
                    const char* field_name,
                    const char* jni_signature) {
  jfieldID field_id = env->GetFieldID(clazz.obj(), field_name, jni_signature);
  CHECK(!base::android::ClearException(env) && field_id) <<
      "Failed to find field " << field_name << " " << jni_signature;
  return field_id;
}
} // namespace

struct XWalkSettings::FieldIds {
  // Note on speed. One may think that an approach that reads field values via
  // JNI is ineffective and should not be used. Please keep in mind that in the
  // legacy WebView the whole Sync method took <1ms on Xoom, and no one is
  // expected to modify settings in performance-critical code.
  FieldIds() = default;

  explicit FieldIds(JNIEnv* env) {
    const char* kStringClassName = "Ljava/lang/String;";

    // FIXME: we should be using a new GetFieldIDFromClassName() with caching.
    ScopedJavaLocalRef<jclass> clazz(
        GetClass(env, "com/tenta/xwalk/refactor/XWalkSettings"));
    allow_scripts_to_close_windows =
        GetFieldID(env, clazz, "mAllowScriptsToCloseWindows", "Z");
    load_images_automatically =
        GetFieldID(env, clazz, "mLoadsImagesAutomatically", "Z");
    images_enabled =
        GetFieldID(env, clazz, "mImagesEnabled", "Z");
    java_script_enabled =
        GetFieldID(env, clazz, "mJavaScriptEnabled", "Z");
    allow_universal_access_from_file_urls =
        GetFieldID(env, clazz, "mAllowUniversalAccessFromFileURLs", "Z");
    allow_file_access_from_file_urls =
        GetFieldID(env, clazz, "mAllowFileAccessFromFileURLs", "Z");
    java_script_can_open_windows_automatically =
        GetFieldID(env, clazz, "mJavaScriptCanOpenWindowsAutomatically", "Z");
    support_multiple_windows =
        GetFieldID(env, clazz, "mSupportMultipleWindows", "Z");
    dom_storage_enabled =
        GetFieldID(env, clazz, "mDomStorageEnabled", "Z");
    database_enabled =
        GetFieldID(env, clazz, "mDatabaseEnabled", "Z");
    use_wide_viewport =
        GetFieldID(env, clazz, "mUseWideViewport", "Z");
    media_playback_requires_user_gesture =
        GetFieldID(env, clazz, "mMediaPlaybackRequiresUserGesture", "Z");
    default_video_poster_url =
        GetFieldID(env, clazz, "mDefaultVideoPosterURL", kStringClassName);
    text_size_percent =
        GetFieldID(env, clazz, "mTextSizePercent", "I");
    default_font_size =
        GetFieldID(env, clazz, "mDefaultFontSize", "I");
    default_fixed_font_size =
        GetFieldID(env, clazz, "mDefaultFixedFontSize", "I");
    spatial_navigation_enabled =
        GetFieldID(env, clazz, "mSpatialNavigationEnabled", "Z");
    quirks_mode_enabled =
        GetFieldID(env, clazz, "mQuirksModeEnabled", "Z");
    initialize_at_minimum_page_scale =
        GetFieldID(env, clazz, "mLoadWithOverviewMode", "Z");
    viewport_meta_enabled =
        GetFieldID(env, clazz, "mViewPortMetaEnabled", "Z");
  }

  // Field ids
  jfieldID allow_scripts_to_close_windows;
  jfieldID load_images_automatically;
  jfieldID images_enabled;
  jfieldID java_script_enabled;
  jfieldID allow_universal_access_from_file_urls;
  jfieldID allow_file_access_from_file_urls;
  jfieldID java_script_can_open_windows_automatically;
  jfieldID support_multiple_windows;
  jfieldID dom_storage_enabled;
  jfieldID database_enabled;
  jfieldID use_wide_viewport;
  jfieldID media_playback_requires_user_gesture;
  jfieldID default_video_poster_url;
  jfieldID text_size_percent;
  jfieldID default_font_size;
  jfieldID default_fixed_font_size;
  jfieldID spatial_navigation_enabled;
  jfieldID quirks_mode_enabled;
  jfieldID initialize_at_minimum_page_scale;
  jfieldID viewport_meta_enabled;
};

class XWalkSettingsUserData : public base::SupportsUserData::Data {
 public:
  explicit XWalkSettingsUserData(XWalkSettings* ptr) : settings_(ptr) {}

  static XWalkSettings* GetSettings(content::WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    XWalkSettingsUserData* data = static_cast<XWalkSettingsUserData*>(
        web_contents->GetUserData(kXWalkSettingsUserDataKey));
    return data ? data->settings_ : NULL;
  }

 private:
  XWalkSettings* settings_;
};

XWalkSettings::XWalkSettings(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      xwalk_settings_(env, obj),
      _javascript_can_open_windows_automatically(false),
      renderer_prefs_initialized_(false),
      allow_third_party_cookies_(false) {

  web_contents->SetUserData(kXWalkSettingsUserDataKey,
                            std::make_unique<XWalkSettingsUserData>(this));
}

XWalkSettings::~XWalkSettings() {
  if (web_contents()) {
    web_contents()->SetUserData(kXWalkSettingsUserDataKey, NULL);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_obj = xwalk_settings_.get(env);
  if (!scoped_obj.obj())
    return;
  Java_XWalkSettings_nativeXWalkSettingsGone(env, scoped_obj, reinterpret_cast<intptr_t>(this));
}

bool XWalkSettings::GetAllowThirdPartyCookies() {
  return allow_third_party_cookies_;
}

void XWalkSettings::Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

void XWalkSettings::WebContentsDestroyed() {
  delete this;
}

/**
 *
 */
XWalkSettings* XWalkSettings::FromWebContents(content::WebContents* web_contents) {
  return XWalkSettingsUserData::GetSettings(web_contents);
}

XWalkRenderViewHostExt* XWalkSettings::GetXWalkRenderViewHostExt() {
  if (!web_contents()) return NULL;
  XWalkContent* contents = XWalkContent::FromWebContents(web_contents());
  if (!contents) return NULL;
  return contents->render_view_host_ext();
}

void XWalkSettings::UpdateEverything() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = xwalk_settings_.get(env);
  if (!scoped_obj.obj()) return;

  Java_XWalkSettings_updateEverything(env, scoped_obj);
}

void XWalkSettings::UpdateEverythingLocked(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  UpdateInitialPageScale(env, obj);
  UpdateWebkitPreferencesLocked(env, obj);
  UpdateUserAgent(env, obj);
  ResetScrollAndScaleState(env, obj);
  UpdateFormDataPreferences(env, obj);
  UpdateRendererPreferencesLocked(env, obj);
//  UpdateOffscreenPreRasterLocked(env, obj);
}

void XWalkSettings::UpdateUserAgent(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (!web_contents()) return;

  ScopedJavaLocalRef<jstring> str =
      Java_XWalkSettings_getUserAgentLocked(env, obj);
  bool ua_overidden = str.obj() != NULL;

  if (ua_overidden) {
    std::string override = base::android::ConvertJavaStringToUTF8(str);
    web_contents()->SetUserAgentOverride(override, true /*override_in_new_tabs*/);
  }

  content::NavigationController& controller =
      web_contents()->GetController();
  for (int i = 0; i < controller.GetEntryCount(); ++i)
    controller.GetEntryAtIndex(i)->SetIsOverridingUserAgent(ua_overidden);
}

void XWalkSettings::PopulateWebPreferences(content::WebPreferences* webPrefs) {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = xwalk_settings_.get(env);
  if (scoped_obj.is_null())
    return;
  // Grab the lock and call PopulateWebPreferencesLocked.
  Java_XWalkSettings_populateWebPreferences(env, scoped_obj, reinterpret_cast<jlong>(webPrefs));

  LOG(INFO) << "iotto " << __func__ << " accelerated_video_decode_enabled=" << webPrefs->accelerated_video_decode_enabled;
}

void XWalkSettings::PopulateWebPreferencesLocked(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj,
                                                 jlong webPrefsPtr) {
  if (!web_contents())
    return;
  content::WebPreferences* web_prefs = reinterpret_cast<WebPreferences*>(webPrefsPtr);

  PopulateFixedWebPreferences(web_prefs);

  web_prefs->text_autosizing_enabled = Java_XWalkSettings_getTextAutosizingEnabledLocked(env, obj);

  {
    XWalkRenderViewHostExt* render_view_host_ext = GetXWalkRenderViewHostExt();
    if (!render_view_host_ext)
      return;

    int text_size_percent = Java_XWalkSettings_getTextSizePercentLocked(env, obj);
    if (web_prefs->text_autosizing_enabled) {
      web_prefs->font_scale_factor = text_size_percent / 100.0f;
      web_prefs->force_enable_zoom = text_size_percent >= 130;
      // Use the default zoom factor value when Text Autosizer is turned on.
      render_view_host_ext->SetTextZoomFactor(1);
    } else {
      web_prefs->force_enable_zoom = false;
      render_view_host_ext->SetTextZoomFactor(text_size_percent / 100.0f);
    }
  }

//  web_prefs->standard_font_family_map[content::kCommonScript] =
//      ConvertJavaStringToUTF16(
//          Java_AwSettings_getStandardFontFamilyLocked(env, obj));
//
//  web_prefs->fixed_font_family_map[content::kCommonScript] =
//      ConvertJavaStringToUTF16(
//          Java_AwSettings_getFixedFontFamilyLocked(env, obj));
//
//  web_prefs->sans_serif_font_family_map[content::kCommonScript] =
//      ConvertJavaStringToUTF16(
//          Java_AwSettings_getSansSerifFontFamilyLocked(env, obj));
//
//  web_prefs->serif_font_family_map[content::kCommonScript] =
//      ConvertJavaStringToUTF16(
//          Java_AwSettings_getSerifFontFamilyLocked(env, obj));
//
//  web_prefs->cursive_font_family_map[content::kCommonScript] =
//      ConvertJavaStringToUTF16(
//          Java_AwSettings_getCursiveFontFamilyLocked(env, obj));
//
//  web_prefs->fantasy_font_family_map[content::kCommonScript] =
//      ConvertJavaStringToUTF16(
//          Java_AwSettings_getFantasyFontFamilyLocked(env, obj));
//
//  web_prefs->default_encoding = ConvertJavaStringToUTF8(
//      Java_AwSettings_getDefaultTextEncodingLocked(env, obj));
//
//  web_prefs->minimum_font_size =
//      Java_AwSettings_getMinimumFontSizeLocked(env, obj);
//
//  web_prefs->minimum_logical_font_size =
//      Java_AwSettings_getMinimumLogicalFontSizeLocked(env, obj);
//
//  web_prefs->default_font_size =
//      Java_AwSettings_getDefaultFontSizeLocked(env, obj);
//
//  web_prefs->default_fixed_font_size =
//      Java_AwSettings_getDefaultFixedFontSizeLocked(env, obj);

  // Blink's LoadsImagesAutomatically and ImagesEnabled must be
  // set cris-cross to Android's. See
  // https://code.google.com/p/chromium/issues/detail?id=224317#c26
  web_prefs->loads_images_automatically = Java_XWalkSettings_getImagesEnabledLocked(env, obj);
  web_prefs->images_enabled = Java_XWalkSettings_getLoadsImagesAutomaticallyLocked(env, obj);

  web_prefs->javascript_enabled = Java_XWalkSettings_getJavaScriptEnabledLocked(env, obj);

  web_prefs->allow_universal_access_from_file_urls = Java_XWalkSettings_getAllowUniversalAccessFromFileURLsLocked(env,
                                                                                                                  obj);

  web_prefs->allow_file_access_from_file_urls = Java_XWalkSettings_getAllowFileAccessFromFileURLsLocked(env, obj);

  _javascript_can_open_windows_automatically = Java_XWalkSettings_getJavaScriptCanOpenWindowsAutomaticallyLocked(env,
                                                                                                                 obj);

  web_prefs->supports_multiple_windows = Java_XWalkSettings_getSupportMultipleWindowsLocked(env, obj);

  web_prefs->plugins_enabled = false;

  web_prefs->application_cache_enabled = Java_XWalkSettings_getAppCacheEnabledLocked(env, obj);

  web_prefs->local_storage_enabled = Java_XWalkSettings_getDomStorageEnabledLocked(env, obj);

  web_prefs->databases_enabled = Java_XWalkSettings_getDatabaseEnabledLocked(env, obj);

  web_prefs->wide_viewport_quirk = true;
  web_prefs->use_wide_viewport = Java_XWalkSettings_getUseWideViewportLocked(env, obj);

  web_prefs->force_zero_layout_height = Java_XWalkSettings_getForceZeroLayoutHeightLocked(env, obj);

  const bool zero_layout_height_disables_viewport_quirk =
      Java_XWalkSettings_getZeroLayoutHeightDisablesViewportQuirkLocked(env, obj);
  web_prefs->viewport_enabled = !(zero_layout_height_disables_viewport_quirk && web_prefs->force_zero_layout_height);

  web_prefs->double_tap_to_zoom_enabled = Java_XWalkSettings_supportsDoubleTapZoomLocked(env, obj);

  web_prefs->initialize_at_minimum_page_scale = Java_XWalkSettings_getLoadWithOverviewModeLocked(env, obj);

  web_prefs->autoplay_policy =
      Java_XWalkSettings_getMediaPlaybackRequiresUserGestureLocked(env, obj) ?
          content::AutoplayPolicy::kUserGestureRequired : content::AutoplayPolicy::kNoUserGestureRequired;

  ScopedJavaLocalRef<jstring> url = Java_XWalkSettings_getDefaultVideoPosterURLLocked(env, obj);
  web_prefs->default_video_poster_url = url.obj() ? GURL(ConvertJavaStringToUTF8(url)) : GURL();

  bool support_quirks = Java_XWalkSettings_getSupportLegacyQuirksLocked(env, obj);
  // Please see the corresponding Blink settings for bug references.
  web_prefs->support_deprecated_target_density_dpi = support_quirks;
  web_prefs->use_legacy_background_size_shorthand_behavior = support_quirks;
  web_prefs->viewport_meta_merge_content_quirk = support_quirks;
  web_prefs->viewport_meta_non_user_scalable_quirk = support_quirks;
  web_prefs->viewport_meta_zero_values_quirk = support_quirks;
  web_prefs->clobber_user_agent_initial_scale_quirk = support_quirks;
  web_prefs->ignore_main_frame_overflow_hidden_quirk = support_quirks;
  web_prefs->report_screen_size_in_physical_pixels_quirk = support_quirks;

  web_prefs->reuse_global_for_unowned_main_frame = Java_XWalkSettings_getAllowEmptyDocumentPersistenceLocked(env, obj);

  web_prefs->password_echo_enabled = Java_XWalkSettings_getPasswordEchoEnabledLocked(env, obj);

  web_prefs->spatial_navigation_enabled = Java_XWalkSettings_getSpatialNavigationLocked(env, obj);

  bool enable_supported_hardware_accelerated_features =
      Java_XWalkSettings_getEnableSupportedHardwareAcceleratedFeaturesLocked(env, obj);

  bool accelerated_2d_canvas_enabled_by_switch = web_prefs->accelerated_2d_canvas_enabled;
  web_prefs->accelerated_2d_canvas_enabled = true;
  if (!accelerated_2d_canvas_enabled_by_switch || !enable_supported_hardware_accelerated_features) {
    // Any canvas smaller than this will fallback to software. Abusing this
    // slightly to turn canvas off without changing
    // accelerated_2d_canvas_enabled, which also affects compositing mode.
    // Using 100M instead of max int to avoid overflows.
    web_prefs->minimum_accelerated_2d_canvas_size = 100 * 1000 * 1000;
  }
//  // If strict mixed content checking is enabled then running should not be
//    // allowed.
//    DCHECK(!Java_AwSettings_getUseStricMixedContentCheckingLocked(env, obj) ||
//           !Java_AwSettings_getAllowRunningInsecureContentLocked(env, obj));
//    web_prefs->allow_running_insecure_content =
//        Java_AwSettings_getAllowRunningInsecureContentLocked(env, obj);
//    web_prefs->strict_mixed_content_checking =
//        Java_AwSettings_getUseStricMixedContentCheckingLocked(env, obj);
//
  web_prefs->fullscreen_supported = Java_XWalkSettings_getFullscreenSupportedLocked(env, obj);
//    web_prefs->record_whole_document =
//        Java_AwSettings_getRecordFullDocument(env, obj);
//
//    // TODO(jww): This should be removed once sufficient warning has been given of
//    // possible API breakage because of disabling insecure use of geolocation.
  web_prefs->allow_geolocation_on_insecure_origins = Java_XWalkSettings_getAllowGeolocationOnInsecureOrigins(env, obj);

  web_prefs->do_not_update_selection_on_mutating_selection_range =
      Java_XWalkSettings_getDoNotUpdateSelectionOnMutatingSelectionRange(env, obj);

  // default true
    web_prefs->css_hex_alpha_color_enabled = false;
//        Java_AwSettings_getCSSHexAlphaColorEnabledLocked(env, obj);

  // Keep spellcheck disabled on html elements unless the spellcheck="true"
  // attribute is explicitly specified. This "opt-in" behavior is for backward
  // consistency in apps that use WebView (see crbug.com/652314).
  web_prefs->spellcheck_enabled_by_default = false;

  // default true
  web_prefs->scroll_top_left_interop_enabled = false;
//        Java_AwSettings_getScrollTopLeftInteropEnabledLocked(env, obj);

  LOG(WARNING) << __func__ << " force_dark_mode";
//  switch (Java_AwSettings_getForceDarkModeLocked(env, obj)) {
//    case ForceDarkMode::FORCE_DARK_OFF:
//      web_prefs->force_dark_mode_enabled = false;
//      break;
//    case ForceDarkMode::FORCE_DARK_ON:
//      web_prefs->force_dark_mode_enabled = true;
//      break;
//    case ForceDarkMode::FORCE_DARK_AUTO: {
//      AwContents* contents = AwContents::FromWebContents(web_contents());
//      web_prefs->force_dark_mode_enabled =
//          contents && contents->GetViewTreeForceDarkState();
//      break;
//    }
//  }

  // Blink's behavior is that if the preferred color scheme matches the
  // supported color scheme, then force dark will be disabled, otherwise
  // the preferred color scheme will be reset to no preference. Therefore
  // when enabling force dark, we also set the preferred color scheme to
  // dark so that dark themed content will be preferred over force darkening.
  web_prefs->preferred_color_scheme =
      web_prefs->force_dark_mode_enabled
          ? blink::PreferredColorScheme::kDark
          : blink::PreferredColorScheme::kNoPreference;
}

bool XWalkSettings::GetAllowFileAccess() {
  // TODO(timvolodine): cache this lazily on update, crbug.com/949590
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = xwalk_settings_.get(env);
  if (scoped_obj.is_null())
    return true;
  return Java_XWalkSettings_getAllowFileAccess(env, scoped_obj);
}

void XWalkSettings::UpdateWebkitPreferencesLocked(JNIEnv* env, const JavaParamRef<jobject>& obj) {

  if (!web_contents()) return;
  XWalkRenderViewHostExt* render_view_host_ext = GetXWalkRenderViewHostExt();
  if (!render_view_host_ext) return;

  if (!field_ids_)
    field_ids_.reset(new FieldIds(env));

  content::RenderViewHost* render_view_host =
      web_contents()->GetRenderViewHost();
  if (!render_view_host) return;

  render_view_host->OnWebkitPreferencesChanged();
//
//  content::WebPreferences prefs = render_view_host->GetWebkitPreferences();
//
////  LOG(INFO) << "XWalkSettings::UpdateWebkitPreferences";
//
//  prefs.allow_scripts_to_close_windows =
//      env->GetBooleanField(obj, field_ids_->allow_scripts_to_close_windows);
//
//  // Blink's LoadsImagesAutomatically and ImagesEnabled must be
//  // set cris-cross to Android's. See
//  // https://code.google.com/p/chromium/issues/detail?id=224317#c26
//  prefs.images_enabled =
//      env->GetBooleanField(obj, field_ids_->load_images_automatically);
//  prefs.loads_images_automatically =
//      env->GetBooleanField(obj, field_ids_->images_enabled);
//
//  prefs.javascript_enabled =
//      env->GetBooleanField(obj, field_ids_->java_script_enabled);
//
//  prefs.allow_universal_access_from_file_urls = env->GetBooleanField(
//      obj, field_ids_->allow_universal_access_from_file_urls);
//
//  prefs.allow_file_access_from_file_urls = env->GetBooleanField(
//      obj, field_ids_->allow_file_access_from_file_urls);
//
//  _javascript_can_open_windows_automatically = env->GetBooleanField(
//      obj, field_ids_->java_script_can_open_windows_automatically);
//
//  prefs.supports_multiple_windows = env->GetBooleanField(
//      obj, field_ids_->support_multiple_windows);
//
//  prefs.application_cache_enabled =
//      Java_XWalkSettings_getAppCacheEnabled(env, obj);
//
//  prefs.local_storage_enabled = env->GetBooleanField(
//      obj, field_ids_->dom_storage_enabled);
//
//  prefs.databases_enabled = env->GetBooleanField(
//      obj, field_ids_->database_enabled);
//
//  prefs.initialize_at_minimum_page_scale =
//      env->GetBooleanField(obj, field_ids_->initialize_at_minimum_page_scale);
//  prefs.double_tap_to_zoom_enabled = prefs.use_wide_viewport =
//      env->GetBooleanField(obj, field_ids_->use_wide_viewport);
//
////  prefs.user_gesture_required_for_media_playback = env->GetBooleanField(
////      obj, field_ids_->media_playback_requires_user_gesture);
//
//  prefs.password_echo_enabled =
//      Java_XWalkSettings_getPasswordEchoEnabledLocked(env, obj);
//
//  prefs.double_tap_to_zoom_enabled =
//      Java_XWalkSettings_supportsDoubleTapZoomLocked(env, obj);
//
//  prefs.spatial_navigation_enabled = env->GetBooleanField(
//      obj, field_ids_->spatial_navigation_enabled);
//
//  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
//  prefs.allow_running_insecure_content =
//      command_line->HasSwitch(switches::kAllowRunningInsecureContent);
//
//  ScopedJavaLocalRef<jstring> str;
//  str.Reset(
//      env, static_cast<jstring>(
//          env->GetObjectField(obj, field_ids_->default_video_poster_url)));
//  prefs.default_video_poster_url = str.obj() ?
//      GURL(ConvertJavaStringToUTF8(str)) : GURL();
//
//  int text_size_percent = env->GetIntField(obj, field_ids_->text_size_percent);
//
//  prefs.text_autosizing_enabled =
//      Java_XWalkSettings_getTextAutosizingEnabledLocked(env, obj);
//  if (prefs.text_autosizing_enabled) {
//    prefs.font_scale_factor = text_size_percent / 100.0f;
//    prefs.force_enable_zoom = text_size_percent >= 130;
//    // Use the default zoom factor value when Text Autosizer is turned on.
//    render_view_host_ext->SetTextZoomFactor(1);
//  } else {
//    prefs.force_enable_zoom = false;
//    render_view_host_ext->SetTextZoomFactor(text_size_percent / 100.0f);
//  }
//
//  int font_size = env->GetIntField(obj, field_ids_->default_font_size);
//  prefs.default_font_size = font_size;
//
//  int fixed_font_size = env->GetIntField(obj,
//      field_ids_->default_fixed_font_size);
//  prefs.default_fixed_font_size = fixed_font_size;
//
//  bool support_quirks = env->GetBooleanField(
//      obj, field_ids_->quirks_mode_enabled);
//  prefs.viewport_meta_non_user_scalable_quirk = support_quirks;
//  prefs.clobber_user_agent_initial_scale_quirk = support_quirks;
//
//  prefs.wide_viewport_quirk = true;
//  prefs.viewport_meta_enabled = env->GetBooleanField(obj, field_ids_->viewport_meta_enabled);
//
//  render_view_host->UpdateWebkitPreferences(prefs);
}

void XWalkSettings::UpdateCookiePolicyLocked(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  allow_third_party_cookies_ =
      Java_XWalkSettings_getAcceptThirdPartyCookiesLocked(env, obj);
}

void XWalkSettings::UpdateFormDataPreferences(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;
  XWalkContent* content = XWalkContent::FromWebContents(web_contents());
  if (!content)
    return;
  content->SetSaveFormData(Java_XWalkSettings_getSaveFormDataLocked(env, obj));
}

void XWalkSettings::UpdateRendererPreferencesLocked(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  bool update_prefs = false;
  blink::mojom::RendererPreferences* prefs =
      web_contents()->GetMutableRendererPrefs();

  if (!renderer_prefs_initialized_) {
    content::UpdateFontRendererPreferencesFromSystemSettings(prefs);
    renderer_prefs_initialized_ = true;
    update_prefs = true;
  }

  if (prefs->accept_languages.compare(XWalkContentBrowserClient::GetAcceptLangsImpl()))
  {
    prefs->accept_languages = XWalkContentBrowserClient::GetAcceptLangsImpl();
    update_prefs = true;
  }

  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (update_prefs && host)
    host->SyncRendererPrefs();

  if (update_prefs && base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // make sure to update accept languages when the network service is enabled
    XWalkBrowserContext* browser_context = XWalkBrowserContext::FromWebContents(web_contents());
    // AndroidWebview does not use per-site storage partitions.
    content::StoragePartition* storage_partition = content::BrowserContext::GetDefaultStoragePartition(browser_context);
    std::string expanded_language_list = net::HttpUtil::ExpandLanguageList(prefs->accept_languages);
    storage_partition->GetNetworkContext()->SetAcceptLanguage(
        net::HttpUtil::GenerateAcceptLanguageHeader(expanded_language_list));
  }
}
/**
 *
 */
bool XWalkSettings::GetJavaScriptCanOpenWindowsAutomatically() {
  return _javascript_can_open_windows_automatically;
}

void XWalkSettings::RenderViewHostChanged(content::RenderViewHost* old_host, content::RenderViewHost* new_host) {
//  LOG(INFO) << "XWalkSettings::RenderViewCreated";
  // A single WebContents can normally have 0 to many RenderViewHost instances
  // associated with it.
  // This is important since there is only one RenderViewHostExt instance per
  // WebContents (and not one RVHExt per RVH, as you might expect) and updating
  // settings via RVHExt only ever updates the 'current' RVH.
  // In android_webview we don't swap out the RVH on cross-site navigations, so
  // we shouldn't have to deal with the multiple RVH per WebContents case. That
  // in turn means that the newly created RVH is always the 'current' RVH
  // (since we only ever go from 0 to 1 RVH instances) and hence the DCHECK.
  DCHECK(web_contents()->GetRenderViewHost() == new_host);

  UpdateEverything();
}

// TODO(iotto) : Remove
void XWalkSettings::RenderFrameForInterstitialPageCreated(content::RenderFrameHost* render_frame_host) {
//  LOG(INFO) << "XWalkSettings::RenderFrameForInterstitialPageCreated";
  UpdateEverything();
}

void XWalkSettings::UpdateAcceptLanguages(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  LOG(INFO) << "iotto " << __func__;

  PrefService* pref_service = GetPrefs();
  if (!pref_service) return;
  pref_service->SetString(
      "intl.accept_languages",
      base::android::ConvertJavaStringToUTF8(
          Java_XWalkSettings_getAcceptLanguagesLocked(env, obj)));
}

PrefService* XWalkSettings::GetPrefs() {
  return user_prefs::UserPrefs::Get(XWalkBrowserContext::GetDefault());
}

static jlong JNI_XWalkSettings_Init(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 const JavaParamRef<jobject>& web_contents) {
  content::WebContents* contents = content::WebContents::FromJavaWebContents(
      web_contents);
  XWalkSettings* settings = new XWalkSettings(env, obj, contents);
  return reinterpret_cast<intptr_t>(settings);
}

static ScopedJavaLocalRef<jstring>
JNI_XWalkSettings_GetDefaultUserAgent(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, GetUserAgent());
}

void XWalkSettings::UpdateInitialPageScale(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;
  XWalkRenderViewHostExt* render_view_host_ext = GetXWalkRenderViewHostExt();
  if (!render_view_host_ext)
    return;

  float initial_page_scale_percent = Java_XWalkSettings_getInitialPageScalePercentLocked(env, obj);
  if (initial_page_scale_percent == 0) {
    render_view_host_ext->SetInitialPageScale(-1);
    return;
  }
  float dip_scale = static_cast<float>(Java_XWalkSettings_getDIPScaleLocked(env, obj));
  render_view_host_ext->SetInitialPageScale(initial_page_scale_percent / dip_scale / 100.0f);
}

void XWalkSettings::ResetScrollAndScaleState(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  XWalkRenderViewHostExt* rvhe = GetXWalkRenderViewHostExt();
  if (!rvhe)
    return;
  rvhe->ResetScrollAndScaleState();
}

bool RegisterXWalkSettings(JNIEnv* env) {
//  return RegisterNativesImpl(env);
  return false;
}

}  // namespace xwalk
