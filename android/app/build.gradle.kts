plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}
// Gradle property wins over environment. See README.md for LAN/release commands.
fun setting(name: String, fallback: String) = providers.gradleProperty(name)
    .orElse(providers.environmentVariable(name)).getOrElse(fallback).trim()
fun quoted(value: String) = "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\""
val apiUrl = setting("FALLSAFE_API_BASE_URL", "http://10.0.2.2:3000/api/v1/").trimEnd('/') + "/"
android {
    namespace = "vn.nckh27pa.fallsafe"
    compileSdk = 36
    buildToolsVersion = "36.0.0"
    defaultConfig {
        applicationId = "vn.nckh27pa.fallsafe"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.1-demo"
        buildConfigField("String", "API_BASE_URL", quoted(apiUrl))
        buildConfigField("String", "FALLSAFE_DEVICE_ID", quoted(setting("FALLSAFE_DEVICE_ID", "PHONE-DEFAULT")))
        buildConfigField("String", "FALLSAFE_USER_ID", quoted(setting("FALLSAFE_USER_ID", "user-01")))
        buildConfigField("String", "FALLSAFE_ESP32_DEVICE_ID", quoted(setting("FALLSAFE_ESP32_DEVICE_ID", "FALLSAFE-01A2")))
    }
    buildFeatures { compose = true; buildConfig = true }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    sourceSets["main"].java.srcDir("../core/src")
    sourceSets["test"].resources.srcDir("../../docs/fixtures")
}
dependencies {
    implementation(platform("androidx.compose:compose-bom:2025.08.01"))
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui-tooling-preview")
    debugImplementation("androidx.compose.ui:ui-tooling")
    implementation("com.google.code.gson:gson:2.11.0")
    implementation("com.squareup.retrofit2:retrofit:2.11.0")
    implementation("com.squareup.retrofit2:converter-gson:2.11.0")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.9.3")
    implementation("androidx.lifecycle:lifecycle-process:2.9.3")
    testImplementation("junit:junit:4.13.2")
    testImplementation("com.squareup.okhttp3:mockwebserver:4.12.0")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.10.2")
}
