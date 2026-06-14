#!/usr/bin/env swift
import Foundation

struct Options {
    var baseURL = "http://127.0.0.1:8890"
    var token = "immersive-dev-token"
    var command = "demo"
}

func parseOptions() -> Options {
    var options = Options()
    var index = 1
    let args = CommandLine.arguments
    while index < args.count {
        let arg = args[index]
        func takeValue() -> String? {
            guard index + 1 < args.count else { return nil }
            index += 1
            return args[index]
        }

        switch arg {
        case "--base":
            if let value = takeValue() { options.baseURL = value }
        case "--token":
            if let value = takeValue() { options.token = value }
        case "--command":
            if let value = takeValue() { options.command = value }
        case "health", "start", "state", "handoff", "ready", "reset", "demo":
            options.command = arg
        default:
            if arg.hasPrefix("--base=") {
                options.baseURL = String(arg.dropFirst("--base=".count))
            } else if arg.hasPrefix("--token=") {
                options.token = String(arg.dropFirst("--token=".count))
            } else if arg.hasPrefix("--command=") {
                options.command = String(arg.dropFirst("--command=".count))
            }
        }
        index += 1
    }
    return options
}

func request(_ method: String, _ path: String, body: String? = nil,
             options: Options) throws -> String {
    guard let url = URL(string: options.baseURL + path) else {
        throw NSError(domain: "SwiftFakeRelayClient", code: 1,
                      userInfo: [NSLocalizedDescriptionKey: "Invalid URL"])
    }

    var request = URLRequest(url: url)
    request.httpMethod = method
    request.setValue(options.token, forHTTPHeaderField: "X-STK-Relay-Token")
    if let body = body {
        request.httpBody = Data(body.utf8)
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
    }

    let semaphore = DispatchSemaphore(value: 0)
    var resultData: Data?
    var resultResponse: URLResponse?
    var resultError: Error?

    URLSession.shared.dataTask(with: request) { data, response, error in
        resultData = data
        resultResponse = response
        resultError = error
        semaphore.signal()
    }.resume()

    semaphore.wait()

    if let error = resultError {
        throw error
    }

    if let http = resultResponse as? HTTPURLResponse,
       !(200...299).contains(http.statusCode) {
        let payload = resultData.flatMap { String(data: $0, encoding: .utf8) } ?? ""
        throw NSError(domain: "SwiftFakeRelayClient", code: http.statusCode,
                      userInfo: [NSLocalizedDescriptionKey:
                        "HTTP \(http.statusCode): \(payload)"])
    }

    return resultData.flatMap { String(data: $0, encoding: .utf8) } ?? ""
}

func startBody() -> String {
    return """
    {
      "session_id": "tholus-fake-swift-001",
      "preset": "standard_arcade",
      "track_id": "snowtuxpeak",
      "handoff_policy": "lap_based_handoff",
      "player_count": 6,
      "arcade_ui": true,
      "players": [
        {"player_id": "p1", "display_name": "Player 1"},
        {"player_id": "p2", "display_name": "Player 2"},
        {"player_id": "p3", "display_name": "Player 3"},
        {"player_id": "p4", "display_name": "Player 4"},
        {"player_id": "p5", "display_name": "Player 5"},
        {"player_id": "p6", "display_name": "Player 6"}
      ]
    }
    """
}

let options = parseOptions()

do {
    switch options.command {
    case "health":
        print(try request("GET", "/immersive/v1/health", options: options))
    case "start":
        print(try request("POST", "/immersive/v1/session/start",
                          body: startBody(), options: options))
    case "state":
        print(try request("GET", "/immersive/v1/state", options: options))
    case "handoff":
        print(try request("POST", "/immersive/v1/operator/handoff",
                          body: #"{"player_id":"p2"}"#, options: options))
    case "ready":
        print(try request("POST", "/immersive/v1/operator/handoff",
                          body: #"{"command":"skip_handoff"}"#, options: options))
    case "reset":
        print(try request("POST", "/immersive/v1/session/reset",
                          body: "{}", options: options))
    case "demo":
        print(try request("GET", "/immersive/v1/health", options: options))
        print(try request("POST", "/immersive/v1/session/start",
                          body: startBody(), options: options))
        print(try request("GET", "/immersive/v1/state", options: options))
        print(try request("POST", "/immersive/v1/operator/handoff",
                          body: #"{"player_id":"p2"}"#, options: options))
        print(try request("POST", "/immersive/v1/session/reset",
                          body: "{}", options: options))
    default:
        fputs("Unknown command. Use health, start, state, handoff, ready, reset, or demo.\n", stderr)
        exit(2)
    }
} catch {
    fputs("SwiftFakeRelayClient error: \(error.localizedDescription)\n", stderr)
    exit(1)
}
