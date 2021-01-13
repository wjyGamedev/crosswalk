package com.tenta.xwalk.refactor;

import org.chromium.base.UserDataHost;

public class XWalkTab {

    private final UserDataHost mUserDataHost = new UserDataHost();
    
    /**
     * @return {@link UserDataHost} that manages {@link UserData} objects attached to
     *         this Tab instance.
     */
    public UserDataHost getUserDataHost() {
        return mUserDataHost;
    }
}
