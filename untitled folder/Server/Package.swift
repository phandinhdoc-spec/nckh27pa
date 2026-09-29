// swift-tools-version: 6.4

import PackageDescription

let package = Package(
    name: "ThienNhanServer",

    platforms: [
        .macOS(.v26)
    ],

    products: [
        .executable(
            name: "ThienNhanServer",
            targets: ["ThienNhanServer"]
        )
    ],

    dependencies: [
        .package(
            url: "https://github.com/hummingbird-project/hummingbird.git",
            from: "2.0.0"
        )
    ],

    targets: [
        .executableTarget(
            name: "ThienNhanServer",
            dependencies: [
                .product(
                    name: "Hummingbird",
                    package: "hummingbird"
                )
            ],
            path: "Sources/ThienNhanServer"
        ),
        .testTarget(
            name: "ThienNhanServerTests",
            dependencies: ["ThienNhanServer"]
        )
    ]
)
