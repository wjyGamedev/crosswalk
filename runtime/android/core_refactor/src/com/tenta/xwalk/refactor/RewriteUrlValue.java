package com.tenta.xwalk.refactor;

public class RewriteUrlValue {
    private String url;
    private int transitionType;
    
    private void nativeInit(String url, int trType) {
        this.url = url;
        this.transitionType = trType;
    }
    
    public String getUrl() {
        return url;
    }
    
    public void setUrl(String url) {
        this.url = url;
    }
    
    public int getTransitionType() {
        return transitionType;
    }
    
    public void setTransitionType(int transitionType) {
        this.transitionType = transitionType;
    }
    
    
}
