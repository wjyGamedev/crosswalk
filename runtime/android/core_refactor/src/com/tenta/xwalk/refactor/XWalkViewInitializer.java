
package com.tenta.xwalk.refactor;

import java.io.File;

import android.app.Activity;
import android.content.Context;
import android.os.Environment;

public class XWalkViewInitializer {

    public static void DoInit(Context context) {
        XWalkViewDelegate.init(null, context);

        // TODO(iotto) : Fix or drop extensions
        // if (!CommandLine.getInstance().hasSwitch("disable-xwalk-extensions")) {
        // BuiltinXWalkExtensions.load(mContext);
        // } else {
        XWalkPreferences.setValue(XWalkPreferences.ENABLE_EXTENSIONS, false);
        // }

        XWalkPathHelper.initialize();
        XWalkPathHelper.setCacheDirectory(context.getApplicationContext().getCacheDir().getPath());

        String state = Environment.getExternalStorageState();
        if (Environment.MEDIA_MOUNTED.equals(state)
                || Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
            File extCacheDir = context.getApplicationContext().getExternalCacheDir();
            if (null != extCacheDir) {
                XWalkPathHelper.setExternalCacheDirectory(extCacheDir.getPath());
            }
        }
    }
}
