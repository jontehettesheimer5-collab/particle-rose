# Particle Rose

An offline iPhone app with a white “made by 32e4” credit on black, followed by a pink particle rose. The credit fades in over 0.6 seconds, holds for one second, and fades out over 0.6 seconds. The rose blooms, turns, dissolves, and repeats every 11 seconds. The credit plays once each time the app launches.

The animation is an original procedural approximation of the supplied reference video. It is silent and requires no accounts, network requests, or permissions.

## Build the iPhone app

Upload this folder's contents to the root of a GitHub repository. Include `.github/workflows/build-ios.yml`. Open Actions, choose “Build iPhone IPA”, then “Run workflow”. A push to `main` also starts a build.

When the build succeeds, open the run and download `ParticleRose-unsigned-IPA` under Artifacts. Extract the download to obtain `ParticleRose-unsigned.ipa`. Import the IPA into your signing app and sign it with a valid certificate and provisioning profile compatible with your device.

The IPA is unsigned. Installation requires signing. A cloud build has not been verified until the GitHub workflow finishes successfully.

## Preview

Serve `web` with any static HTTP server and open `index.html` in a browser. The same files run inside the installed iPhone app.

## Build locally on a Mac

Install Xcode and XcodeGen, run `xcodegen generate`, then open `ParticleRose.xcodeproj` in Xcode.
