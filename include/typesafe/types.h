// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace typesafe
{

/**
 * @brief Represents a categorical multiple-choice question.
 *
 * The model will select exactly one option from the provided criteria.
 */
struct Choice
{
		std::optional<nlohmann::json>                        instructions;
		std::map<std::string, std::optional<nlohmann::json>> criteria;
};

/**
 * @brief Represents a scoring or grading question.
 *
 * The model places the state at a position along the ordered levels in
 * criteria; the answer may land between two levels.
 */
struct Score
{
		std::optional<nlohmann::json> instructions;
		std::vector<nlohmann::json>   criteria;
};

/**
 * @brief Semantic criteria specifying the meaning of true and false for a Noul
 * question.
 */
struct NoulCriteria
{
		std::optional<nlohmann::json> true_meaning;
		std::optional<nlohmann::json> false_meaning;
};

/**
 * @brief A yes/no question, answered with the probability that the answer
 * is yes.
 */
struct Noul
{
		std::optional<nlohmann::json> instructions;
		std::optional<NoulCriteria>   criteria;
};

/**
 * @brief The model's response to a Choice question.
 */
struct ChoiceAnswer
{
		std::string choice;  ///< The selected key from criteria.
		double      confidence = 0.0;
		std::map<std::string, double> probabilities;
};

/**
 * @brief The model's response to a Score question.
 */
struct ScoreAnswer
{
		double                        score = 0.0;
		double                        confidence = 0.0;
		std::map<int, nlohmann::json> legend;
		std::map<int, double>         probabilities;
};

/**
 * @brief The model's response to a Noul question.
 */
struct NoulAnswer
{
		double noul = 0.0;
};

/**
 * @brief Token usage for the request.
 */
struct Usage
{
		std::int64_t input_tokens = 0;
		std::int64_t output_tokens = 0;
};

/**
 * @brief The primary request payload for the System One API.
 *
 * Encapsulates the context (state) and the specific structural questions
 * (choices, scores, nouls) the model needs to answer.
 */
struct SystemOneRequest
{
		nlohmann::json
		    state;  ///< The contextual state or document to evaluate.
		std::optional<std::string>
		    model;  ///< The specific model version to invoke (optional).
		std::optional<int> timeout_ms;  ///< Per-request timeout override.

		std::map<std::string, nlohmann::json>
		    questions;  ///< The requested questions.
		std::map<std::string, nlohmann::json>
		    extra_body;  ///< Custom fields to append to the JSON body.
		std::map<std::string, std::string>
		    extra_headers;  ///< Custom headers to append to the HTTP request.

		/**
		 * @brief Registers a Choice question.
		 * @param key The JSON key for the answer in the response.
		 * @param q The Choice configuration.
		 */
		void add(const std::string &key, const Choice &q);

		/**
		 * @brief Registers a Score question.
		 * @param key The JSON key for the answer in the response.
		 * @param q The Score configuration.
		 */
		void add(const std::string &key, const Score &q);

		/**
		 * @brief Registers a Noul question.
		 * @param key The JSON key for the answer in the response.
		 * @param q The Noul configuration.
		 */
		void add(const std::string &key, const Noul &q);
};

/**
 * @brief Decoded response from the System One API endpoint.
 */
struct SystemOneResponse
{
		std::string                         model;
		Usage                               usage;
		std::map<std::string, ChoiceAnswer> choices;
		std::map<std::string, ScoreAnswer>  scores;
		std::map<std::string, NoulAnswer>   nouls;
};

/**
 * @brief Model descriptive metadata returned by GET /v1/models.
 */
struct ModelMetadata
{
		std::string name;
		std::string description;
		std::string release_date;
};

/**
 * @brief Response container for GET /v1/models listing available models.
 */
struct ListModelsResponse
{
		std::vector<ModelMetadata> models;
};

/** @brief Serializes Choice question to JSON. */
void to_json(nlohmann::json &j, const Choice &q);

/** @brief Serializes Score question to JSON. */
void to_json(nlohmann::json &j, const Score &q);

/** @brief Serializes Noul question to JSON. */
void to_json(nlohmann::json &j, const Noul &q);

/** @brief Serializes NoulCriteria to JSON. */
void to_json(nlohmann::json &j, const NoulCriteria &criteria);

/** @brief Deserializes ChoiceAnswer from JSON. */
void from_json(const nlohmann::json &j, ChoiceAnswer &r);

/** @brief Deserializes ScoreAnswer from JSON. */
void from_json(const nlohmann::json &j, ScoreAnswer &r);

/** @brief Deserializes NoulAnswer from JSON. */
void from_json(const nlohmann::json &j, NoulAnswer &r);

/** @brief Deserializes Usage statistics from JSON. */
void from_json(const nlohmann::json &j, Usage &r);

/** @brief Deserializes SystemOneResponse from JSON. */
void from_json(const nlohmann::json &j, SystemOneResponse &r);

/** @brief Deserializes ModelMetadata from JSON. */
void from_json(const nlohmann::json &j, ModelMetadata &r);

/** @brief Deserializes ListModelsResponse from JSON. */
void from_json(const nlohmann::json &j, ListModelsResponse &r);

}  // namespace typesafe
