workspace "SmartLinks C++ Microservice" "C4 Component diagram for SmartLinks microservice" {

    model {
        # External Systems
        apiGateway = softwareSystem "API Gateway" "Nginx reverse proxy that routes requests to microservices" "External"
        database = softwareSystem "MongoDB" "NoSQL database storing links, rules, and user data" "External"
        jwtService = softwareSystem "JWT Service" "Node.js microservice providing JWT authentication and JWKS endpoint" "External"

        # SmartLinks System
        smartLinksSystem = softwareSystem "SmartLinks System" "Smart link redirection system with DSL rules and JWT authorization" {

            # SmartLinks Container (C++ Microservice)
            smartLinksApp = container "SmartLinks Microservice" "Processes redirect requests using DSL rules" "C++ / Drogon" {

                # HTTP Layer
                httpHandler = component "HTTP Handler" "Handles incoming HTTP requests and manages request scopes" "DrogonMiddlewareAdapter"

                # Middleware Pipeline
                middleware405 = component "Method Not Allowed Middleware" "Validates HTTP method (GET/HEAD only)" "Middleware"
                middleware404 = component "Not Found Middleware" "Checks if link exists and not deleted" "Middleware"
                middleware422 = component "Unprocessable Middleware" "Validates link is not frozen" "Middleware"
                middleware307 = component "Redirect Middleware" "Evaluates DSL rules and performs redirect" "Middleware"

                # Business Logic
                dslParser = component "DSL Parser" "Parses DSL rules text into Abstract Syntax Tree" "Parser"
                dslEvaluator = component "DSL Evaluator" "Evaluates AST nodes with request context" "Evaluator"
                jwtValidator = component "JWT Validator" "Validates JWT tokens using RS256 and JWKS public keys" "jwt-cpp"

                # Plugin System
                authorizedAdapter = component "AUTHORIZED Adapter" "Plugin that checks JWT token validity" "Plugin Adapter"
                languageAdapter = component "LANGUAGE Adapter" "Plugin that checks Accept-Language header" "Plugin Adapter"
                datetimeAdapter = component "DATETIME Adapter" "Plugin that checks current time against time windows" "Plugin Adapter"

                # Core Infrastructure
                iocContainer = component "IoC Container" "Dependency injection container with hierarchical scopes" "DI Container"
                context = component "Context" "Stores request data for DSL evaluation" "Data Storage"

                # Data Access
                mongoRepository = component "MongoDB Repository" "Provides low-level access to MongoDB collections" "mongocxx"

                # Relationships - HTTP Flow
                httpHandler -> iocContainer "Creates request scope"
                httpHandler -> middleware405 "Executes middleware chain"
                middleware405 -> middleware404 "Next in chain"
                middleware404 -> middleware422 "Next in chain"
                middleware422 -> middleware307 "Next in chain"

                # Middleware dependencies
                middleware404 -> mongoRepository "Checks link existence"
                middleware422 -> mongoRepository "Checks link state"
                middleware307 -> dslParser "Parses DSL rules"
                middleware307 -> dslEvaluator "Evaluates AST"
                middleware307 -> context "Populates with request data"
                middleware307 -> mongoRepository "Loads DSL rules"

                # DSL Engine
                dslParser -> authorizedAdapter "Creates adapter nodes"
                dslParser -> languageAdapter "Creates adapter nodes"
                dslParser -> datetimeAdapter "Creates adapter nodes"
                dslEvaluator -> authorizedAdapter "Evaluates conditions"
                dslEvaluator -> languageAdapter "Evaluates conditions"
                dslEvaluator -> datetimeAdapter "Evaluates conditions"

                # Plugin adapters
                authorizedAdapter -> jwtValidator "Validates JWT token"
                authorizedAdapter -> context "Reads request data"
                languageAdapter -> context "Reads Accept-Language"
                datetimeAdapter -> context "Reads current time"

                # IoC provides instances
                iocContainer -> middleware405 "Provides instance"
                iocContainer -> middleware404 "Provides instance"
                iocContainer -> middleware422 "Provides instance"
                iocContainer -> middleware307 "Provides instance"
                iocContainer -> jwtValidator "Provides instance"
                iocContainer -> context "Provides instance"
            }
        }

        # External relationships
        apiGateway -> smartLinksApp "Routes HTTP requests to" "HTTP"
        mongoRepository -> database "Reads/writes data" "mongocxx driver"
        jwtValidator -> jwtService "Fetches JWKS public keys" "HTTPS"
    }

    views {
        component smartLinksApp "SmartLinksComponents" {
            include *
            autoLayout lr

            # Custom styling
            title "SmartLinks C++ Microservice - Component Diagram (C4 Level 3)"
            description "Detailed component view showing internal architecture of SmartLinks microservice"
        }

        # Define styles
        styles {
            element "Software System" {
                background #1168bd
                color #ffffff
                shape RoundedBox
            }
            element "Container" {
                background #438dd5
                color #ffffff
                shape RoundedBox
            }
            element "Component" {
                background #85bbf0
                color #000000
                shape Component
            }
            element "External" {
                background #999999
                color #ffffff
            }
            element "DrogonMiddlewareAdapter" {
                background #f9a825
                color #000000
            }
            element "Middleware" {
                background #ec407a
                color #ffffff
            }
            element "Parser" {
                background #ab47bc
                color #ffffff
            }
            element "Evaluator" {
                background #ab47bc
                color #ffffff
            }
            element "jwt-cpp" {
                background #7e57c2
                color #ffffff
            }
            element "Plugin Adapter" {
                background #26c6da
                color #000000
            }
            element "DI Container" {
                background #66bb6a
                color #000000
            }
            element "Data Storage" {
                background #9ccc65
                color #000000
            }
            element "mongocxx" {
                background #ffee58
                color #000000
            }
        }

        theme default
    }

}
