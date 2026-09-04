<?php

/**
 * @generate-function-entries
 * @generate-class-entries
 */

class Vrzno
{
    public function __construct(mixed ...$args) {}

    public function __get(string $property_name): mixed {}

    public function __call(string $method_name, array $args): mixed {}

    public function __invoke(mixed ...$args): mixed {}

    public function __toString(): string {}
}

function vrzno_eval(string $code): string {}

function vrzno_run(string $global_function_name, array $args = []): string {}

function vrzno_timeout(int $milliseconds, callable $callback): void {}

function vrzno_await(Vrzno $promise_like): mixed {}

function vrzno_env(string $name): mixed {}

function vrzno_shared(string $name): mixed {}

function vrzno_import(string $module_url): Vrzno {}

function vrzno_target(Vrzno $value): int {}

/** @internal */
function vrzno_zval(mixed $value): int {}
