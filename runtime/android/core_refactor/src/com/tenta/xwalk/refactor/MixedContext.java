// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.tenta.xwalk.refactor;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;

import android.view.LayoutInflater;

/**
 * MixedContext provides ApplicationContext for the contextImpl object
 * created by Context.CreatePackageContext().
 *
 * For cross package usage, the library part need the possibility to
 * get both the application's context and the library itself's context.
 *
 */
class MixedContext extends ContextWrapper {
    private Context mActivityCtx;

    public MixedContext(Context base, Context activity) {
        super(base);
        mActivityCtx = activity;
    }

    @Override
    public Context getApplicationContext() {
        return mActivityCtx.getApplicationContext();
    }

    @Override
    public boolean bindService(Intent in, ServiceConnection conn, int flags) {
        return getApplicationContext().bindService(in, conn, flags);
    }

    @Override
    public void unbindService(ServiceConnection conn) {
        getApplicationContext().unbindService(conn);
    }

        @Override
        public ClassLoader getClassLoader() {
            final ClassLoader appCl = getBaseContext().getClassLoader();
            final ClassLoader webViewCl = this.getClass().getClassLoader();
            return new ClassLoader() {
                @Override
                protected Class<?> findClass(String name) throws ClassNotFoundException {
                    // First look in the WebViewProvider class loader.
                    try {
                        return webViewCl.loadClass(name);
                    } catch (ClassNotFoundException e) {
                        // Look in the app class loader; allowing it to throw
                        // ClassNotFoundException.
                        return appCl.loadClass(name);
                    }
                }
            };
        }

    @Override
    public Object getSystemService(String name) {
            if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                LayoutInflater i = (LayoutInflater) getBaseContext().getSystemService(name);
                return i.cloneInContext(this);
            } else {
                return getBaseContext().getSystemService(name);
            }

/*        if (name.equals(Context.LAYOUT_INFLATER_SERVICE)) {
            return super.getSystemService(name);
        }
        return mActivityCtx.getSystemService(name);
*/
    }

        @Override
        public void registerComponentCallbacks(ComponentCallbacks callback) {
            // We have to override registerComponentCallbacks and unregisterComponentCallbacks
            // since they call getApplicationContext().[un]registerComponentCallbacks()
            // which causes us to go into a loop.
            getBaseContext().registerComponentCallbacks(callback);
        }

        @Override
        public void unregisterComponentCallbacks(ComponentCallbacks callback) {
            getBaseContext().unregisterComponentCallbacks(callback);
        }

}
