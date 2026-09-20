# Android Thingy

### A Shitty Enterprise™ Product

A WebView in an APK. Opens the fucking website and lets the frontend do its job.
Yup... ja*a happened

## Build the shit

Get JDK 17, Android SDK Platform 36, and Build Tools 35.0.0. Accept the SDK
licenses and set `ANDROID_HOME` (or `sdk.dir` in `local.properties`).
Android Studio can handle the SDK shit too.

From `Client/`:

```sh
cp server.properties.example server.properties
# Put your actual HTTPS domain in server.properties, then:
./gradlew assembleDebug lintDebug
```

Use `https://your-domain/`, optionally with a port. No paths or query shit.
Config stays out of Git; the URL still ends up in the APK.
Windows? Use `gradlew.bat`.

Your APK: `app/build/outputs/apk/debug/app-debug.apk`. Copy it to your phone
and install. Needs Android 8.0+ and an updated Android System WebView.

## Connection error screens

The client has built-in fuck-up indicators for connection/DNS failures(including `ERR_NAME_NOT_RESOLVED`) and server errors (HTTP 5xx, including the HTTP 530

Only main-page failures from the configured server trigger these screens
failed images, scripts, or API requests do not replace the page

To check on a device or emulator:

1. Disable Wi-Fi and mobile data put it on the airplane mode or something then launch the app Check that connection
   guidance appears instead of the WebView error page.

2. Restore internet and tap **Try again**. Check that the original page opens.

3. With the device online, stop the server's Cloudflare tunnel and reload the
   app. Check that **Server unavailable** replaces Cloudflare's error page.

4. Restart the tunnel and tap **Try again**. Check that the app recovers.

5. Rotate the device on each error screen, then retry. Check that the error
   screen remains usable. Check that Back still navigates or exits normally.

## Shit to know

> Jukebox is getting dope ngl...

Use your deployed HTTPS domain. `localhost` means...yk...the phone

Self-signed certs aren't trusted. Server needs to be reachable

Login and UI come from the websit 
Cookies(yummy OwO (yeah that's my max web knowledge)) are separate from your browser.

Shit **kinda** works...

Fuck...BRUH THIS REPO NEEDS 10 FUCKING READMES MORE
