// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/types.h"

#include <charconv>
#include <nlohmann/json.hpp>

namespace typesafe
{

void to_json(nlohmann::json &j, const Choice &q)
{
	j = nlohmann::json{{"type", "choice"}, {"criteria", q.criteria}};
	if (q.instructions)
		j["instructions"] = *q.instructions;
}

void to_json(nlohmann::json &j, const Score &q)
{
	j = nlohmann::json{{"type", "score"}, {"criteria", q.criteria}};
	if (q.instructions)
		j["instructions"] = *q.instructions;
}

void to_json(nlohmann::json &j, const Noul &q)
{
	j = nlohmann::json{{"type", "noul"}};
	if (q.instructions)
		j["instructions"] = *q.instructions;
	if (q.criteria)
		j["criteria"] = *q.criteria;
}

void to_json(nlohmann::json &j, const NoulCriteria &criteria)
{
	j = nlohmann::json::object();
	if (criteria.true_meaning)
		j["true"] = *criteria.true_meaning;
	if (criteria.false_meaning)
		j["false"] = *criteria.false_meaning;
}

void SystemOneRequest::add(const std::string &key, const Choice &q)
{
	questions[key] = q;
}

void SystemOneRequest::add(const std::string &key, const Score &q)
{
	questions[key] = q;
}

void SystemOneRequest::add(const std::string &key, const Noul &q)
{
	questions[key] = q;
}

void from_json(const nlohmann::json &j, ChoiceAnswer &r)
{
	j.at("choice").get_to(r.choice);
	j.at("confidence").get_to(r.confidence);
	j.at("probabilities").get_to(r.probabilities);
}

void from_json(const nlohmann::json &j, ScoreAnswer &r)
{
	j.at("score").get_to(r.score);
	j.at("confidence").get_to(r.confidence);

	const auto &legend = j.at("legend");
	const auto &probabilities = j.at("probabilities");
	if (!legend.is_object() || !probabilities.is_object())
		throw nlohmann::json::type_error::create(
		    302, "score legend and probabilities must be objects", &j);

	r.legend.clear();
	r.probabilities.clear();
	for (const auto &[key, value] : legend.items())
	{
		int index = 0;
		auto [end, error]
		    = std::from_chars(key.data(), key.data() + key.size(), index);
		if (error != std::errc{} || end != key.data() + key.size())
			throw nlohmann::json::type_error::create(
			    302, "score legend keys must be integers", &legend);
		if (!(value.is_string() || value.is_object() || value.is_array()))
			throw nlohmann::json::type_error::create(
			    302, "score legend values must be strings, objects, or arrays",
			    &legend);
		r.legend[index] = value;
	}
	for (const auto &[key, value] : probabilities.items())
	{
		int index = 0;
		auto [end, error]
		    = std::from_chars(key.data(), key.data() + key.size(), index);
		if (error != std::errc{} || end != key.data() + key.size())
			throw nlohmann::json::type_error::create(
			    302, "score probability keys must be integers", &probabilities);
		r.probabilities[index] = value.get<double>();
	}
}

void from_json(const nlohmann::json &j, NoulAnswer &r)
{
	j.at("noul").get_to(r.noul);
}

void from_json(const nlohmann::json &j, Usage &r)
{
	j.at("input_tokens").get_to(r.input_tokens);
	j.at("output_tokens").get_to(r.output_tokens);
}

void from_json(const nlohmann::json &j, SystemOneResponse &r)
{
	j.at("model").get_to(r.model);
	j.at("usage").get_to(r.usage);

	const auto &answers = j.at("answers");
	if (!answers.is_object())
		throw nlohmann::json::type_error::create(
		    302, "answers must be an object", &answers);
	if (answers.empty())
		throw nlohmann::json::other_error::create(
		    501, "answers must contain at least one answer", &answers);

	for (const auto &[name, raw] : answers.items())
	{
		if (!raw.contains("type") || !raw.at("type").is_string())
			throw nlohmann::json::type_error::create(
			    302, "answer type must be a string", &raw);

		const std::string type = raw.at("type").get<std::string>();
		if (type == "choice")
			r.choices[name] = raw.get<ChoiceAnswer>();
		else if (type == "score")
			r.scores[name] = raw.get<ScoreAnswer>();
		else if (type == "noul")
			r.nouls[name] = raw.get<NoulAnswer>();
	}
}

void from_json(const nlohmann::json &j, ModelMetadata &r)
{
	j.at("name").get_to(r.name);
	j.at("description").get_to(r.description);
	j.at("release_date").get_to(r.release_date);
}

void from_json(const nlohmann::json &j, ListModelsResponse &r)
{
	j.at("models").get_to(r.models);
}

}  // namespace typesafe
