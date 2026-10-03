package com.google.android.gms.common.api;

import android.content.Context;

/**
 * Compiled as PlayBuilder, then renamed to GoogleApiClient$Builder so it
 * shadows the Play Services class. The real build() waits forever.
 */
public class PlayBuilder {
    private final GoogleApiClient.ConnectionCallbacks connected;
    private final GoogleApiClient.OnConnectionFailedListener failed;

    public PlayBuilder(Context context) {
        this.connected = null;
        this.failed = null;
    }

    public PlayBuilder(Context context, GoogleApiClient.ConnectionCallbacks connected,
            GoogleApiClient.OnConnectionFailedListener failed) {
        this.connected = connected;
        this.failed = failed;
    }

    public PlayBuilder addApi(Api<?> api) {
        return this;
    }

    public <O extends Api.ApiOptions.HasOptions> PlayBuilder addApi(Api<O> api, O options) {
        return this;
    }

    public PlayBuilder addScope(Scope scope) {
        return this;
    }

    public PlayBuilder addConnectionCallbacks(GoogleApiClient.ConnectionCallbacks callbacks) {
        return this;
    }

    public PlayBuilder addOnConnectionFailedListener(GoogleApiClient.OnConnectionFailedListener listener) {
        return this;
    }

    public PlayBuilder setHandler(android.os.Handler handler) {
        return this;
    }

    public PlayBuilder setViewForPopups(android.view.View view) {
        return this;
    }

    public PlayBuilder setGravityForPopups(int gravity) {
        return this;
    }

    public GoogleApiClient build() {
        System.out.println("play services offline");
        return new OfflineClient(connected, failed);
    }
}
