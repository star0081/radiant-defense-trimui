package com.google.android.gms.common.api;

import android.support.v4.app.FragmentActivity;
import com.google.android.gms.common.ConnectionResult;
import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.concurrent.TimeUnit;

/** Stand-in client. Play Services is not on the device, so connect fails at once. */
public final class OfflineClient extends GoogleApiClient {
    private final ConnectionCallbacks connected;
    private final OnConnectionFailedListener failed;
    private boolean told;

    OfflineClient(ConnectionCallbacks connected, OnConnectionFailedListener failed) {
        this.connected = connected;
        this.failed = failed;
    }

    public ConnectionResult blockingConnect() {
        return new ConnectionResult(ConnectionResult.SERVICE_INVALID);
    }

    public ConnectionResult blockingConnect(long timeout, TimeUnit unit) {
        return blockingConnect();
    }

    public PendingResult<Status> clearDefaultAccountAndReconnect() {
        return null;
    }

    public void connect() {
        connect(1);
    }

    public void connect(int mode) {
        if (told) {
            return;
        }
        told = true;
        if (failed != null) {
            failed.onConnectionFailed(new ConnectionResult(ConnectionResult.SERVICE_INVALID));
        }
    }

    public void disconnect() {
    }

    public void dump(String prefix, FileDescriptor fd, PrintWriter writer, String[] args) {
    }

    public ConnectionResult getConnectionResult(Api<?> api) {
        return new ConnectionResult(ConnectionResult.SERVICE_INVALID);
    }

    public boolean hasConnectedApi(Api<?> api) {
        return false;
    }

    public boolean isConnected() {
        return false;
    }

    public boolean isConnecting() {
        return false;
    }

    public boolean isConnectionCallbacksRegistered(ConnectionCallbacks callbacks) {
        return callbacks == connected;
    }

    public boolean isConnectionFailedListenerRegistered(OnConnectionFailedListener listener) {
        return listener == failed;
    }

    public void reconnect() {
        told = false;
        connect(1);
    }

    public void registerConnectionCallbacks(ConnectionCallbacks callbacks) {
    }

    public void registerConnectionFailedListener(OnConnectionFailedListener listener) {
    }

    public void stopAutoManage(FragmentActivity activity) {
    }

    public void unregisterConnectionCallbacks(ConnectionCallbacks callbacks) {
    }

    public void unregisterConnectionFailedListener(OnConnectionFailedListener listener) {
    }
}
